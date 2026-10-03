/*
* UAE - The Un*x Amiga Emulator
*
* Toccata Z2 board emulation, playback only
*
* Copyright 2014-2015 Toni Wilen
*
* Taken from WinUAE's sndboard.cpp and cut down to the Toccata and its AD1848
* codec. Recording, the Prelude boards and the FM801 PCI cards are left out.
*
* WinUAE hands the codec's samples to its audio mixer as a stream. That mixer
* does not exist here, so the board keeps its own clock instead: the FIFO is
* drained at the codec's rate in emulated time, and every sample Paula puts out
* gets the codec's output at that instant added to it (sound.h). Both then
* share the same buffer, the same pacing and the same rate control.
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <math.h>

#ifdef TOCCATA

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#include "events.h"
#include "autoconf.h"
#include "devices.h"
#include "audio.h"
#include "sndboard.h"

#define DEBUG_SNDDEV 0

#define BOARD_SIZE 65536
#define BOARD_MASK (BOARD_SIZE - 1)

#define FIFO_SIZE_MAX 1024

/* The codec's samples are resampled to the instants Paula puts hers out with
   a windowed sinc. Paula's rate is never exactly the codec's - the sound rate
   control moves it by up to 0.4% - so those instants slide past the codec's
   samples. Linear interpolation dulls the high frequencies the more, the
   nearer an instant falls to the middle between two samples, and the sliding
   turned that into a hiss on cymbals and the like. The sinc's response stays
   flat to about 20 kHz wherever the instant falls. */
#define RS_TAPS 96
#define RS_PHASES 256              /* table steps between two codec samples */
#define RS_KAISER_BETA 7.86        /* about 80 dB of stopband */
/* The pass band ends short of Nyquist, so that the stop band starts at it:
   centred on Nyquist, the images of what lies just below - plenty in some
   mp3s - came through. Flat to about 20 kHz at 44.1 kHz. */
#define RS_CUTOFF 0.477
/* Paula's samples come in a batch at the end of each stretch update_audio()
   works through - about one per scanline - by which time the codec has run
   on. Taking the codec's output as of then gave every sample of the batch
   the same value, a staircase at the scanline rate that folded everything
   above about 8 kHz back down as noise. The history keeps this many codec
   samples more, so the output can be worked out for the instant of each
   sample even when it lies that far before the codec's latest one. */
#define RS_LOOKBACK 16
#define RS_HIST 256                /* power of two, at least RS_TAPS + RS_LOOKBACK */

/* Colour clocks per second of a PAL Amiga, for when sound emulation is off and
   update_sound() never tells us the real figure: the driver still waits on the
   FIFO, so it has to drain. */
#define DEFAULT_EVENT_CLOCK 3546895.0

struct snddev_data {
	bool enabled;
	uae_u8 acmemory[128];
	int configured;
	uae_u32 baseaddress;
	uae_u8 ad1848_index;
	uae_u8 ad1848_index_mask;
	uae_u8 ad1848_regs[32];
	uae_u8 ad1848_status;
	int autocalibration;
	uae_u8 snddev_status;
	int snddev_irq;
	int fifo_read_index;
	int fifo_write_index;
	int data_in_fifo;
	int fifo_size;
	uae_u8 fifo[FIFO_SIZE_MAX];
	bool fifo_play_byteswap;

	int ch_sample[2];
	/* the codec's recent samples, volume applied, for the resampler */
	float hist[2][RS_HIST];
	unsigned int hist_pos;

	uae_u16 codec_reg1_mask;
	uae_u16 codec_reg1_addr;
	uae_u16 codec_reg2_mask;
	uae_u16 codec_reg2_addr;
	uae_u16 codec_fifo_mask;
	uae_u16 codec_fifo_addr;

	int fifo_half;
	int snddev_active;
	int left_volume, right_volume;

	int freq;
	int play_channels, play_samplebits;
	int play_bytespersample;

	/* our replacement for WinUAE's audio stream: the codec's clock */
	int event_time;
	uae_u32 last_cycles;
	uae_s64 cycle_acc;

	addrbank *bank;
};

static struct snddev_data toccata;
static double base_event_clock;

static float rs_coef[RS_PHASES + 1][RS_TAPS];
static double rs_cutoff;

bool sndboard_playing;

/* ZorroII, 64 KB, MacroSystem (18260) product 12 */
static const uae_u8 toccata_autoconfig[16] = { 0xc1, 12, 0, 0, 18260 >> 8, 18260 & 255 };

#define STATUS_ACTIVE 1
#define STATUS_RESET 2
#define STATUS_FIFO_CODEC 4
#define STATUS_FIFO_RECORD 8
#define STATUS_FIFO_PLAY 0x10
#define STATUS_RECORD_INTENA 0x40
#define STATUS_PLAY_INTENA 0x80

#define STATUS_READ_INTREQ 128
#define STATUS_READ_PLAY_HALF 8
#define STATUS_READ_RECORD_HALF 4

void update_sndboard_sound(double clk)
{
	base_event_clock = clk;
}

static void sndboard_rethink(void)
{
	if (toccata.enabled && toccata.snddev_irq)
		safe_interrupt_set(true);
}

static void process_fifo(struct snddev_data *data)
{
	int prev_data_in_fifo = data->data_in_fifo;
	if (data->data_in_fifo >= data->play_bytespersample) {
		uae_s16 v;
		if (data->play_samplebits == 8) {
			v = data->fifo[data->fifo_read_index] << 8;
			v |= data->fifo[data->fifo_read_index];
			data->ch_sample[0] = v;
			if (data->play_channels == 2) {
				v = data->fifo[data->fifo_read_index + 1] << 8;
				v |= data->fifo[data->fifo_read_index + 1];
			}
			data->ch_sample[1] = v;
		} else if (data->play_samplebits == 16) {
			if (data->fifo_play_byteswap) {
				v = data->fifo[data->fifo_read_index + 0] << 8;
				v |= data->fifo[data->fifo_read_index + 1];
			} else {
				v = data->fifo[data->fifo_read_index + 1] << 8;
				v |= data->fifo[data->fifo_read_index + 0];
			}
			data->ch_sample[0] = v;
			if (data->play_channels == 2) {
				if (data->fifo_play_byteswap) {
					v = data->fifo[data->fifo_read_index + 2] << 8;
					v |= data->fifo[data->fifo_read_index + 3];
				} else {
					v = data->fifo[data->fifo_read_index + 3] << 8;
					v |= data->fifo[data->fifo_read_index + 2];
				}
			}
			data->ch_sample[1] = v;
		}
		data->data_in_fifo -= data->play_bytespersample;
		data->fifo_read_index += data->play_bytespersample;
		data->fifo_read_index = data->fifo_read_index % data->fifo_size;
	} else if (data->data_in_fifo > 0) {
		data->data_in_fifo = 0;
	}

	if (data->data_in_fifo < data->fifo_size / 2 && prev_data_in_fifo >= data->fifo_size / 2)
		data->fifo_half |= STATUS_FIFO_PLAY;
}

/* One tick of the codec clock: what WinUAE's audio stream callback does. */
static void codec_tick(struct snddev_data *data)
{
	if (data->snddev_active & STATUS_FIFO_PLAY) {
		// get all bytes at once to prevent fifo going out of sync
		// if fifo has for example 3 bytes remaining but we need 4.
		process_fifo(data);
	}
	/* WinUAE scales ch_sample itself, which an empty FIFO then scales again on
	   every tick; keep the sample as read and scale the copy instead. */
	unsigned int i = data->hist_pos & (RS_HIST - 1);
	data->hist[0][i] = data->ch_sample[0] * (float)data->left_volume / 32768.0f;
	data->hist[1][i] = data->ch_sample[1] * (float)data->right_volume / 32768.0f;
	data->hist_pos++;
	int old = data->snddev_irq;
	if (data->snddev_active && (data->snddev_status & STATUS_FIFO_CODEC)) {
		if ((data->fifo_half & STATUS_FIFO_PLAY) && (data->snddev_status & STATUS_PLAY_INTENA) && (data->snddev_status & STATUS_FIFO_PLAY)) {
			data->snddev_irq |= STATUS_READ_PLAY_HALF;
		}
	}
	if (old != data->snddev_irq)
		sndboard_rethink();
}

/* Runs the codec up to the current emulated time. */
static void codec_catchup(struct snddev_data *data)
{
	uae_u32 now = get_cycles();
	uae_u32 delta = now - data->last_cycles;
	data->last_cycles = now;
	if (!data->snddev_active || data->event_time <= 0)
		return;
	/* after a pause or a long stall, do not replay seconds of ticks at once */
	uae_u32 limit = (uae_u32)data->event_time * 4096;
	if (delta > limit)
		delta = limit;
	data->cycle_acc += delta;
	while (data->cycle_acc >= data->event_time) {
		data->cycle_acc -= data->event_time;
		codec_tick(data);
	}
}

static int get_volume(uae_u8 v)
{
	int out;
	if (v & 0x80) // Mute bit
		return 0;
	out = v & 63;
	out = 64 - out;
	out *= 32768 / 64;
	return out;
}

static void calculate_volume_toccata(struct snddev_data *data)
{
	data->left_volume = get_volume(data->ad1848_regs[6]);
	data->right_volume = get_volume(data->ad1848_regs[7]);
}

static const int freq_crystals[] = {
	// AD1848 documentation says 24.576MHz but photo of board shows 24.582MHz
	// Added later: It seems there are boards that have correct crystal and
	// also boards with wrong crystal..
	// So we can use correct one in emulation.
	24576000,
	16934400
};
static const int freq_dividers[] = {
	3072,
	1536,
	896,
	768,
	448,
	384,
	512,
	2560
};

static double bessel_i0(double x)
{
	double sum = 1.0, term = 1.0, q = x * x / 4.0;
	for (int k = 1; k < 100; k++) {
		term *= q / ((double)k * k);
		sum += term;
		if (term < sum * 1e-12)
			break;
	}
	return sum;
}

/* Fills the resampler's table: for each phase between two codec samples, the
   weights of the RS_TAPS samples around the output instant. The cutoff is a
   fraction of the codec's rate; each row is scaled to sum to 1, so that
   silence with an offset stays silent whatever the phase. */
static void resampler_setup(double cutoff)
{
	if (cutoff == rs_cutoff)
		return;
	rs_cutoff = cutoff;
	const double half = RS_TAPS / 2.0;
	const double i0_beta = bessel_i0(RS_KAISER_BETA);
	for (int p = 0; p <= RS_PHASES; p++) {
		double c[RS_TAPS], sum = 0.0;
		for (int j = 0; j < RS_TAPS; j++) {
			/* the output instant's distance from sample j, in samples */
			double x = (double)p / RS_PHASES + half - 1 - j;
			double s = x == 0.0 ? 2.0 * cutoff : sin(2.0 * M_PI * cutoff * x) / (M_PI * x);
			double w = x / half;
			w = fabs(w) >= 1.0 ? 0.0 : bessel_i0(RS_KAISER_BETA * sqrt(1.0 - w * w)) / i0_beta;
			c[j] = s * w;
			sum += c[j];
		}
		for (int j = 0; j < RS_TAPS; j++)
			rs_coef[p][j] = (float)(c[j] / sum);
	}
}

static void resampler_clear(struct snddev_data *data)
{
	memset(data->hist, 0, sizeof data->hist);
	data->hist_pos = 0;
}

static void codec_setup(struct snddev_data *data)
{
	uae_u8 c = data->ad1848_regs[8];

	data->play_channels = (c & 0x10) ? 2 : 1;
	data->play_samplebits = (c & 0x40) ? 16 : 8;
	data->freq = freq_crystals[c & 1] / freq_dividers[(c >> 1) & 7];
	data->play_bytespersample = (data->play_samplebits / 8) * data->play_channels;

	write_log(_T("SNDDEV start %s freq=%d bits=%d channels=%d\n"),
		(data->snddev_active & STATUS_FIFO_PLAY) ? _T("Play") : _T("Record"),
		data->freq, data->play_samplebits, data->play_channels);
}

static void codec_start(struct snddev_data *data)
{
	/* Only playback is emulated. A program that records gets a codec that
	   never delivers anything, as with no input connected. */
	data->snddev_active = (data->ad1848_regs[9] & 1) ? STATUS_FIFO_PLAY : 0;

	codec_setup(data);

	double clk = base_event_clock > 0 ? base_event_clock : DEFAULT_EVENT_CLOCK;
	data->event_time = (int)(clk * CYCLE_UNIT / data->freq);
	data->last_cycles = get_cycles();
	data->cycle_acc = 0;

	/* Up to Nyquist of the codec, or of the output if that is lower, so
	   that nothing above it folds back into the audible range. */
	double cutoff = RS_CUTOFF;
	if (currprefs.sound_freq > 0 && currprefs.sound_freq < data->freq)
		cutoff = RS_CUTOFF * currprefs.sound_freq / data->freq;
	resampler_setup(cutoff);
	resampler_clear(data);

	sndboard_playing = (data->snddev_active & STATUS_FIFO_PLAY) != 0;
	if (sndboard_playing)
		audio_activate();
}

static void codec_stop(struct snddev_data *data)
{
	sndboard_playing = false;
	data->ch_sample[0] = data->ch_sample[1] = 0;
	resampler_clear(data);
	if (!data->snddev_active)
		return;
	write_log(_T("CODEC stop\n"));
	data->snddev_active = 0;
}

static void sndboard_hsync(void)
{
	struct snddev_data *data = &toccata;

	if (!data->configured)
		return;
	if (data->autocalibration > 0)
		data->autocalibration--;
	codec_catchup(data);
	/* keeps Paula's output running, which the codec's samples ride on */
	if (sndboard_playing)
		audio_activate();
}

static void toccata_put(struct snddev_data *data, uaecptr addr, uae_u8 v)
{
	int idx = data->ad1848_index & data->ad1848_index_mask;

	if (idx >= 16 && !(data->ad1848_regs[12] & 0x40))
		return;

	if ((addr & data->codec_reg1_mask) == data->codec_reg1_addr) {
		// AD1848 register 0
		data->ad1848_index = v;
	} else if ((addr & data->codec_reg2_mask) == data->codec_reg2_addr) {
		// AD1848 register 1
		uae_u8 old = data->ad1848_regs[idx];

		switch(idx)
		{
			case 12:
			// revision (AD1848)
			v = 0x0a;
			break;
			case 8:
			data->fifo_play_byteswap = false;
			v &= ~0x80;
			break;
		}

		data->ad1848_regs[idx] = v;
#if DEBUG_SNDDEV > 0
		write_log(_T("SNDDEV PUT reg %d = %02x PC=%08x\n"), idx, v, M68K_GETPC);
#endif
		switch(idx)
		{
			case 9:
			if (v & 8) // ACI enabled
				data->autocalibration = 50;
			if (!(old & 3) && (v & 3))
				codec_start(data);
			else if ((old & 3) && !(v & 3))
				codec_stop(data);
			break;

			case 6:
			case 7:
				calculate_volume_toccata(data);
			break;
		}
	} else if ((addr & data->codec_fifo_mask) == data->codec_fifo_addr) {
		// FIFO input
		if (data->snddev_status & STATUS_FIFO_PLAY) {
			/* the board plays in emulated time; bring it up to now first */
			codec_catchup(data);
			// 7202LA datasheet says fifo can't overflow
			if (data->data_in_fifo < data->fifo_size) {
				data->fifo[data->fifo_write_index] = v;
				data->fifo_write_index++;
				data->fifo_write_index %= data->fifo_size;
				data->data_in_fifo++;
			}
		}
		data->snddev_irq &= ~STATUS_READ_PLAY_HALF;
		data->fifo_half &= ~STATUS_FIFO_PLAY;
	} else if ((addr & 0x6800) == 0x0000) {
		// Board status
		if (v & STATUS_RESET) {
			codec_stop(data);
			data->snddev_status = 0;
			data->snddev_irq = 0;
			v = 0;
		}
		if (v == STATUS_ACTIVE) {
			data->fifo_write_index = 0;
			data->fifo_read_index = 0;
			data->data_in_fifo = 0;
			data->snddev_status = 0;
			data->snddev_irq = 0;
			data->fifo_half = 0;
		}
		data->snddev_status = v;
#if DEBUG_SNDDEV > 0
		write_log(_T("TOCCATA PUT STATUS %08x %02x %d PC=%08X\n"), addr, v, idx, M68K_GETPC);
#endif
	} else {
		write_log(_T("SNDDEV PUT UNKNOWN %08x PC=%08x\n"), addr, M68K_GETPC);
	}
}

static uae_u8 toccata_get(struct snddev_data *data, uaecptr addr)
{
	int idx = data->ad1848_index & data->ad1848_index_mask;
	uae_u8 v = 0;

	if (idx >= 16 && !(data->ad1848_regs[12] & 0x40))
		return v;

	if ((addr & data->codec_reg1_mask) == data->codec_reg1_addr) {
		// AD1848 register 0
		v = data->ad1848_index;
	} else if ((addr & data->codec_reg2_mask) == data->codec_reg2_addr) {
		// AD1848 register 1
		v = data->ad1848_regs[idx];
		switch (idx)
		{
			case 11:
			if (data->autocalibration > 10 && data->autocalibration < 30)
				data->ad1848_regs[11] |= 0x20;
			else
				data->ad1848_regs[11] &= ~0x20;
			break;
		}
	} else if ((addr & data->codec_fifo_mask) == data->codec_fifo_addr) {
		// FIFO output: nothing is ever recorded
		v = 0;
		data->snddev_irq &= ~STATUS_READ_RECORD_HALF;
		data->fifo_half &= ~STATUS_FIFO_RECORD;
	} else if ((addr & 0x6800) == 0x0000) {
		// Board status
		codec_catchup(data);
		v = STATUS_READ_INTREQ; // active low
		if (data->snddev_irq) {
			v &= ~STATUS_READ_INTREQ;
			v |= data->snddev_irq;
			data->snddev_irq = 0;
		}
#if DEBUG_SNDDEV > 0
		write_log(_T("TOCCATA GET STATUS %08x %02x %d PC=%08X\n"), addr, v, idx, M68K_GETPC);
#endif
	} else {
		write_log(_T("SNDDEV GET UNKNOWN %08x PC=%08x\n"), addr, M68K_GETPC);
	}
	return v;
}

static uae_u32 REGPARAM3 toccata_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 toccata_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 toccata_bget(uaecptr) REGPARAM;
static void REGPARAM3 toccata_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 toccata_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 toccata_bput(uaecptr, uae_u32) REGPARAM;

static addrbank toccata_bank = {
	toccata_lget, toccata_wget, toccata_bget,
	toccata_lput, toccata_wput, toccata_bput,
	default_xlate, default_check, NULL, _T("*"), _T("Toccata"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO, S_READ, S_WRITE
};

static void REGPARAM2 toccata_bput(uaecptr addr, uae_u32 b)
{
	struct snddev_data *data = &toccata;
	if (!data->bank)
		return;
	b &= 0xff;
	addr &= BOARD_MASK;
	if (!data->configured) {
		switch (addr)
		{
			case 0x48:
			map_banks_z2(&toccata_bank, expamem_board_pointer >> 16, BOARD_SIZE >> 16);
			data->configured = 1;
			data->baseaddress = expamem_board_pointer;
			expamem_next(&toccata_bank, NULL);
			break;
			case 0x4c:
			data->configured = -1;
			expamem_shutup(&toccata_bank);
			break;
		}
		return;
	}
	if (data->configured > 0)
		toccata_put(data, addr, b);
}

static void REGPARAM2 toccata_wput(uaecptr addr, uae_u32 b)
{
	toccata_bput(addr + 0, b >> 8);
	toccata_bput(addr + 1, b >> 0);
}

static void REGPARAM2 toccata_lput(uaecptr addr, uae_u32 b)
{
	toccata_bput(addr + 0, b >> 24);
	toccata_bput(addr + 1, b >> 16);
	toccata_bput(addr + 2, b >>  8);
	toccata_bput(addr + 3, b >>  0);
}

static uae_u32 REGPARAM2 toccata_bget(uaecptr addr)
{
	struct snddev_data *data = &toccata;
	uae_u8 v = 0;
	if (!data->bank)
		return v;
	addr &= BOARD_MASK;
	if (!data->configured) {
		if (addr >= sizeof data->acmemory)
			return 0;
		return data->acmemory[addr];
	}
	if (data->configured > 0)
		v = toccata_get(data, addr);
	return v;
}
static uae_u32 REGPARAM2 toccata_wget(uaecptr addr)
{
	uae_u16 v;
	v = toccata_bget(addr) << 8;
	v |= toccata_bget(addr + 1) << 0;
	return v;
}
static uae_u32 REGPARAM2 toccata_lget(uaecptr addr)
{
	uae_u32 v;
	v = toccata_bget(addr) << 24;
	v |= toccata_bget(addr + 1) << 16;
	v |= toccata_bget(addr + 2) << 8;
	v |= toccata_bget(addr + 3) << 0;
	return v;
}

static void ew(uae_u8 *acmemory, int addr, uae_u32 value)
{
	addr &= 0xffff;
	if (addr == 00 || addr == 02 || addr == 0x40 || addr == 0x42) {
		acmemory[addr] = (value & 0xf0);
		acmemory[addr + 2] = (value & 0x0f) << 4;
	} else {
		acmemory[addr] = ~(value & 0xf0);
		acmemory[addr + 2] = ~((value & 0x0f) << 4);
	}
}

static void ad1848_init(struct snddev_data *data)
{
	memset(data->ad1848_regs, 0, sizeof data->ad1848_regs);
	data->ad1848_regs[2] = 0x80;
	data->ad1848_regs[3] = 0x80;
	data->ad1848_regs[4] = 0x80;
	data->ad1848_regs[5] = 0x80;
	data->ad1848_regs[6] = 0x80;
	data->ad1848_regs[7] = 0x80;
	data->ad1848_regs[9] = 0x10;
	data->ad1848_regs[12] = 0x0a;
	data->ad1848_regs[25] = 0xa0;
	data->ad1848_status = 0xcc;
	data->ad1848_index = 0x40;
	data->ad1848_index_mask = 15;
	data->fifo_play_byteswap = false;
}

static void sndboard_reset(int hardreset)
{
	struct snddev_data *data = &toccata;
	codec_stop(data);
	data->snddev_irq = 0;
	data->bank = NULL;
	data->configured = 0;
	data->enabled = false;
}

bool toccata_init(struct autoconfig_info *aci)
{
	/* WinUAE names the board through its expansion ROM table entry, which
	   uae4arm does not have; without a label the bank's "*" is shown. */
	aci->label = _T("Toccata");
	aci->addrbankp = &toccata_bank;
	aci->autoconfigp = toccata_autoconfig;
	device_add_reset(sndboard_reset);
	device_add_hsync(sndboard_hsync);
	device_add_rethink(sndboard_rethink);

	if (!aci->doinit)
		return true;

	struct snddev_data *data = &toccata;

	codec_stop(data);
	data->configured = 0;
	data->fifo_size = 1024;
	data->codec_reg1_mask = 0x6801;
	data->codec_reg1_addr = 0x6001;
	data->codec_reg2_mask = 0x6801;
	data->codec_reg2_addr = 0x6801;
	data->codec_fifo_mask = 0x6800;
	data->codec_fifo_addr = 0x2000;
	data->snddev_status = 0;
	data->snddev_irq = 0;
	data->fifo_half = 0;
	data->fifo_read_index = data->fifo_write_index = data->data_in_fifo = 0;
	memset(data->acmemory, 0xff, sizeof data->acmemory);
	data->enabled = true;
	for (int i = 0; i < 16; i++) {
		uae_u8 b = toccata_autoconfig[i];
		ew(data->acmemory, i * 4, b);
	}
	data->bank = &toccata_bank;
	ad1848_init(data);
	calculate_volume_toccata(data);
	return true;
}

static inline int clamp16(int v)
{
	return v > 32767 ? 32767 : (v < -32768 ? -32768 : v);
}

/* The codec's output at the instant of Paula's sample, delayed by a fixed
   RS_TAPS / 2 codec samples. The phase falls between two rows of the table,
   so both are applied and the results interpolated. */
static void codec_output(struct snddev_data *data, int *l, int *r)
{
	codec_catchup(data);
	if (data->event_time <= 0) {
		*l = *r = 0;
		return;
	}
	/* where the instant lies from the codec's latest sample, in samples:
	   below 1, and below 0 when that sample came after it */
	uae_u32 latest = data->last_cycles - (uae_u32)data->cycle_acc;
	float since = (float)(uae_s32)(audio_sample_cycles - latest) / data->event_time;
	if (since < -RS_LOOKBACK)
		since = -RS_LOOKBACK;
	/* counted from RS_LOOKBACK samples back, to keep it positive: the
	   integer part moves the taps, the rest is the phase */
	float pos = since + RS_LOOKBACK;
	int whole = (int)pos;
	if (whole > RS_LOOKBACK)
		whole = RS_LOOKBACK;
	float phase = (pos - whole) * RS_PHASES;
	int p = (int)phase;
	if (p >= RS_PHASES)
		p = RS_PHASES - 1;
	float f = phase - p;
	const float *c0 = rs_coef[p], *c1 = rs_coef[p + 1];
	unsigned int base = data->hist_pos - RS_TAPS - RS_LOOKBACK + whole;
	float l0 = 0, l1 = 0, r0 = 0, r1 = 0;
	for (int j = 0; j < RS_TAPS; j++) {
		unsigned int i = (base + j) & (RS_HIST - 1);
		float sl = data->hist[0][i], sr = data->hist[1][i];
		l0 += c0[j] * sl;
		l1 += c1[j] * sl;
		r0 += c0[j] * sr;
		r1 += c1[j] * sr;
	}
	*l = (int)lrintf(l0 + (l1 - l0) * f);
	*r = (int)lrintf(r0 + (r1 - r0) * f);
}

void sndboard_mix_stereo(uae_u32 *left, uae_u32 *right)
{
	int l, r;
	codec_output(&toccata, &l, &r);
	*left = (uae_u16)clamp16((uae_s16)*left + l);
	*right = (uae_u16)clamp16((uae_s16)*right + r);
}

void sndboard_mix_mono(uae_u32 *mono)
{
	int l, r;
	codec_output(&toccata, &l, &r);
	*mono = (uae_u16)clamp16((uae_s16)*mono + (l + r) / 2);
}

#endif /* TOCCATA */
