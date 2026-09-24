/*
 * uae4arm - paravirtual AHI sound
 *
 * The host side of the protocol spoken by WinUAE's uae.audio AHI driver. The
 * driver calls the trap at rtarea_base + 0xFFC0 with an opcode in d0.
 *
 * The protocol and its behaviour follow WinUAE's od-win32/ahidsound_dsonly.cpp
 * (Copyright 1997 Mathias Ortmann, 1997-2001 Brian King, 2000-2002 Bernd
 * Roesch; GPLv2). Only playback is carried over. Where WinUAE hands the samples
 * to a DirectSound buffer of their own, here they go into a ring that
 * sound_sdl.cpp mixes into its stream: SDL 1.2 has a single audio device, and
 * Paula already has it.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef AHI

#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "traps.h"
#include "devices.h"
#include "audio.h"
#include "ahi_v1.h"

/* Stereo frames at the host mixer rate, ~1.5 s at 44.1 kHz. A power of two, so
   indexes wrap by mask. */
#define AHI_RING_FRAMES 65536
#define AHI_RING_MASK   (AHI_RING_FRAMES - 1)
/* Blocks kept queued ahead of Paula's output before the driver is asked for
   more. Counted in emulated time, so this only has to cover the driver's own
   reaction time, not the host's. */
#define AHI_TARGET_BLOCKS 4
/* The old measure, the ring alone, for turbo mode where Paula makes nothing. */
#define AHI_TARGET_BLOCKS_REALTIME 6
/* Room left above the target for blocks already asked for but not yet seen. */
#define AHI_HEADROOM_BLOCKS 3

static uae_s16 ring[AHI_RING_FRAMES * 2];
/* Written by one thread each: ring_wr by the emulator, ring_rd by the SDL audio
   callback. Both only ever count up; the fill level is their difference.
   The emulator empties the ring by asking the callback to, never by writing
   ring_rd itself. */
static volatile uae_atomic ring_wr, ring_rd, ring_flush;
/* Where the emulator's ring_wr stood when it asked for the flush: blocks the
   driver hands over before the callback gets to it are kept. */
static volatile uae_atomic ring_flush_pos;
/* ring_rd minus the Paula frames the device had taken at the same moment,
   published by the callback. It only moves when one of the two runs dry or
   Paula's buffers are skipped. */
static volatile uae_atomic mix_offset;
/* The Paula frames the device had taken as of its last callback. */
static volatile uae_atomic paula_consumed_pub;

static volatile uae_atomic ahi_freq;  /* rate the driver opened with, 0 while closed */
static int ahi_blksize;     /* frames in each block the driver hands over */
static int ahi_channels;
static int ahi_bits;

static uae_u32 *blockbuf;
static int blockbuf_size;

/* Linear resampler from ahi_freq to the mixer rate. */
static double rs_pos;
static uae_s16 rs_prev_l, rs_prev_r;

/* Raised and not yet answered with a block; holds off another interrupt until
   the driver has had one block's worth of time. */
static bool irq_pending;
static uae_u32 irq_raised_at;
static int irq_lines;
/* Set when we raise the interrupt, read back by the driver's interrupt server
   (opcode 4) to tell our interrupt from anything else sharing INT6. */
static int intcount;
static bool warned_format;

static void ahi_raise_irq(void)
{
	irq_pending = true;
	irq_raised_at = sound_frames_produced();
	irq_lines = 0;
	intcount = 1;
	INTREQ(0x8000 | 0x2000);
}

static inline uae_u32 ring_fill(void)
{
	return (uae_u32)ring_wr - (uae_u32)ring_rd;
}

static int host_rate(void)
{
	return currprefs.sound_freq > 0 ? currprefs.sound_freq : 44100;
}

/* How many frames one driver block becomes at the mixer rate. */
static int host_block_frames(void)
{
	if (!ahi_freq || !ahi_blksize)
		return 0;
	int n = (int)((uae_s64)ahi_blksize * host_rate() / ahi_freq);
	return n < 64 ? 64 : n;
}

/* How far AHI reaches past the end of what Paula has made. Everything in the
   ring plays after Paula's queued frames, and the callback takes from both at
   the same pace, so this stays put while the device plays and only shrinks as
   the emulation makes more Paula output: it runs on emulated time, the way the
   play position of a real sound card does, and the driver is asked for blocks
   at an even pace however unevenly the device pulls its data. */
static inline int ahi_lead(void)
{
	return (int)((uae_u32)ring_wr - sound_frames_produced() - (uae_u32)mix_offset);
}

static int ahi_lead_now(void)
{
	/* Until the callback flushes the ring for a new open, the offset still
	   describes the old one; count from where the flush will leave it. */
	if (__atomic_load_n(&ring_flush, __ATOMIC_ACQUIRE))
		return (int)((uae_u32)ring_wr - (uae_u32)ring_flush_pos
			- (sound_frames_produced() - (uae_u32)paula_consumed_pub));
	return ahi_lead();
}

static inline bool want_more(void)
{
	int blk = host_block_frames();
	if (!currprefs.turbo_emulation) {
		if (!ring_flush && ring_fill() >= (uae_u32)(AHI_RING_FRAMES - AHI_HEADROOM_BLOCKS * blk))
			return false;
		return ahi_lead_now() < AHI_TARGET_BLOCKS * blk;
	}
	int target = AHI_TARGET_BLOCKS_REALTIME * blk;
	/* A driver opened with very large blocks would otherwise ask for more
	   than the ring holds, and the excess would be dropped. */
	if (target > AHI_RING_FRAMES - AHI_HEADROOM_BLOCKS * blk)
		target = AHI_RING_FRAMES - AHI_HEADROOM_BLOCKS * blk;
	if (target < blk)
		target = blk;
	return ring_fill() < (uae_u32)target;
}

static void push_frame(uae_s16 l, uae_s16 r)
{
	/* Only the emulator writes, and the mixer only ever frees room, so a full
	   ring can only mean the driver is ahead of the pacing - drop, don't block. */
	if (ring_fill() >= AHI_RING_FRAMES)
		return;
	uae_u32 w = (uae_u32)ring_wr & AHI_RING_MASK;
	ring[w * 2] = l;
	ring[w * 2 + 1] = r;
	ring_wr = ring_wr + 1;
}

/* Behind Paula with nothing left to play means the frames for this moment are
   gone for good - on open, or once the driver fell silent long enough for the
   ring to run dry. Fill the gap with silence where it belongs rather than have
   the driver race to catch up on it. Merely behind, with frames still queued,
   is not a gap yet: the device is further back still, so a late block can make
   it in time, and padding then would cut a hole that need not be heard. */
static void realign(bool force)
{
	if (currprefs.turbo_emulation)
		return;
	if (!force && (ring_flush || ring_fill() != 0))
		return;
	int lead = ahi_lead_now();
	if (lead >= 0)
		return;
	int n = -lead;
	if (n > AHI_RING_FRAMES / 4)
		n = AHI_RING_FRAMES / 4;
	for (int i = 0; i < n; i++)
		push_frame(0, 0);
}

/* One input frame at ahi_freq, resampled into zero or more output frames. */
static void resample_frame(uae_s16 l, uae_s16 r)
{
	double step = (double)ahi_freq / host_rate();

	while (rs_pos < 1.0) {
		double f = rs_pos;
		push_frame((uae_s16)(rs_prev_l + (l - rs_prev_l) * f),
		           (uae_s16)(rs_prev_r + (r - rs_prev_r) * f));
		rs_pos += step;
	}
	rs_pos -= 1.0;
	rs_prev_l = l;
	rs_prev_r = r;
}

static void ahi_close(void)
{
	ahi_freq = 0;
	ahi_blksize = 0;
	ring_flush_pos = ring_wr;
	__atomic_store_n(&ring_flush, 1, __ATOMIC_RELEASE);
	rs_pos = 0.0;
	rs_prev_l = rs_prev_r = 0;
}

static int ahi_open(int freq, int blksize, int channels, int bits)
{
	ahi_close();
	if (freq <= 0 || blksize <= 0 || blksize > 65536)
		return 0;

	if (blksize > blockbuf_size) {
		xfree(blockbuf);
		blockbuf = xmalloc(uae_u32, blksize);
		blockbuf_size = blockbuf ? blksize : 0;
		if (!blockbuf)
			return 0;
	}

	ahi_blksize = blksize;
	ahi_channels = channels;
	ahi_bits = bits;
	warned_format = false;
	ahi_freq = freq;

	/* The ring plays alongside what Paula already has queued, so start it
	   that far in: the first block then lands where it belongs instead of the
	   driver having to catch up on Paula's whole queue at once. */
	realign(true);

	write_log(_T("AHI: open %d Hz, %d frames per block, %d channels, %d bits, mixed at %d Hz\n"),
		freq, blksize, channels, bits, host_rate());

	/* WinUAE starts the driver off at once rather than waiting for a position
	   to move - do the same, or nothing ever asks for the first block. */
	ahi_raise_irq();
	return freq;
}

static void ahi_play_block(TrapContext *ctx, uaecptr addr)
{
	if (!ahi_freq)
		return;
	if (ahi_bits != 16) {
		if (!warned_format) {
			write_log(_T("AHI: %d-bit samples are not supported\n"), ahi_bits);
			warned_format = true;
		}
		return;
	}

	trap_get_longs(ctx, blockbuf, addr, ahi_blksize);
	irq_pending = false;

	for (int i = 0; i < ahi_blksize; i++) {
		uae_u32 v = blockbuf[i];
		/* The same split WinUAE ends up with when these longs land in a
		   little-endian 16-bit stereo buffer: low word left, high word right. */
		uae_s16 lo = (uae_s16)(v & 0xffff);
		uae_s16 hi = (uae_s16)(v >> 16);
		if (ahi_channels == 1) {
			resample_frame(lo, lo);
			resample_frame(hi, hi);
		} else {
			resample_frame(lo, hi);
		}
	}

	/* Keeps the sound pipeline running while Paula has nothing to play, the
	   way CD audio does. */
	audio_activate();
}

uae_u32 REGPARAM2 ahi_demux (TrapContext *ctx)
{
	int opcode = trap_get_dreg(ctx, 0);

	switch (opcode)
	{
	case 0:
		return ahi_open(trap_get_dreg(ctx, 2), trap_get_dreg(ctx, 3), 2, 16);

	case 6: /* new open function */
		return ahi_open(trap_get_dreg(ctx, 2), trap_get_dreg(ctx, 3),
			trap_get_dreg(ctx, 4), trap_get_dreg(ctx, 5));

	case 1:
		ahi_close();
		return 0;

	case 2:
		ahi_play_block(ctx, trap_get_areg(ctx, 0));
		return ahi_blksize;

	case 4:
	{
		/* The driver's interrupt server asking whether INT6 was ours. */
		if (!ahi_freq)
			return (uae_u32)-2;
		int i = intcount;
		intcount = 0;
		return i;
	}

	case 5:
		/* The driver polling for whether to hand over more. */
		if (!ahi_freq)
			return 0;
		if (want_more())
			ahi_raise_irq();
		return 1;

	default:
		/* Recording, the Windows clipboard, Enforcer and native DLLs are all
		   WinUAE features with nothing behind them here. */
		return 0;
	}
}

void ahi_mix (uae_s16 *stream, int frames, bool stereo, uae_u32 paula_consumed)
{
	paula_consumed_pub = paula_consumed;
	if (__atomic_load_n(&ring_flush, __ATOMIC_ACQUIRE)) {
		ring_rd = ring_flush_pos;
		mix_offset = (uae_u32)ring_rd - paula_consumed;
		ring_flush = 0;
	}
	if (!ahi_freq) {
		mix_offset = (uae_u32)ring_rd - paula_consumed;
		return;
	}

	uae_u32 avail = ring_fill();
	int n = frames < (int)avail ? frames : (int)avail;

	for (int i = 0; i < n; i++) {
		uae_u32 r = ((uae_u32)ring_rd + i) & AHI_RING_MASK;
		int l = ring[r * 2];
		int rr = ring[r * 2 + 1];
		if (stereo) {
			int a = stream[i * 2] + l;
			int b = stream[i * 2 + 1] + rr;
			stream[i * 2] = a > 32767 ? 32767 : (a < -32768 ? -32768 : a);
			stream[i * 2 + 1] = b > 32767 ? 32767 : (b < -32768 ? -32768 : b);
		} else {
			int a = stream[i] + (l + rr) / 2;
			stream[i] = a > 32767 ? 32767 : (a < -32768 ? -32768 : a);
		}
	}
	ring_rd = ring_rd + n;
	mix_offset = (uae_u32)ring_rd - paula_consumed;
}

static void ahi_hsync (void)
{
	if (!ahi_freq)
		return;
	/* Paula falls asleep after a few quiet seconds and her output clock stops
	   with her. AHI's runs on it, so keep her awake while the driver is open,
	   the way WinUAE's AHI buffer keeps playing whether or not it gets data. */
	if (!currprefs.turbo_emulation)
		audio_activate();
	realign(false);
	/* One interrupt per block wanted, as WinUAE gives one each time its play
	   position moves a block. Raising it again while the driver's task is
	   still mixing only takes time from that task. */
	if (irq_pending && intcount) {
		/* Raised but not yet claimed by the driver's interrupt server. When it
		   comes while the server is still busy with the previous one, Exec
		   clears INTREQ on the way out and takes ours with it - so keep it
		   raised until the server has seen it. */
		if (++irq_lines >= 4) {
			irq_lines = 0;
			INTREQ(0x8000 | 0x2000);
		}
		return;
	}
	if (irq_pending) {
		/* Claimed, and the driver is making the block. Ask again only if it
		   has not come after a block's worth of time.
		   Turbo mode throws Paula's output away, so time runs by lines there. */
		if (currprefs.turbo_emulation ? ++irq_lines < 4
		    : (int)(sound_frames_produced() - irq_raised_at) < host_block_frames())
			return;
	}
	/* Stands in for WinUAE's "the play position moved a block": ask the driver
	   for more while the queue is short. */
	if (want_more())
		ahi_raise_irq();
}

static void ahi_reset (int hardreset)
{
	ahi_close();
}

static void ahi_free (void)
{
	ahi_close();
	xfree(blockbuf);
	blockbuf = NULL;
	blockbuf_size = 0;
}

void ahi_init (void)
{
	device_add_hsync(ahi_hsync);
	device_add_reset(ahi_reset);
	device_add_exit(ahi_free);
}

#endif /* AHI */
