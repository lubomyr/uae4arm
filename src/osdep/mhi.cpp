/*
 * uae4arm - MPEG audio decoding on the host for MHI
 *
 * MHI is the interface of the MPEG audio decoder cards (MAS Player and the
 * like): a player queues buffers of the stream, the decoder hands them back
 * when it is done with them and signals the player, and the card plays the
 * sound on its own. AmigaAMP, HippoPlayer and others play mp3s with it, and
 * leave the 68k free.
 *
 * The mhiuae.library driver (amiga/mhiuae.s) passes the calls on through
 * uaelib function 201. Here mpg123 decodes the stream, and the decoder keeps
 * a clock of its own in emulated time, as the Toccata's codec does: its
 * samples ride on Paula's (resampler.h). A buffer goes back to the player as
 * soon as mpg123 has taken it all in, as the MAS driver does once the chip
 * has; mpg123 is fed only as fast as it plays, so the player stays a few
 * buffers ahead and no further.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <deque>
#include <mpg123.h>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "devices.h"
#include "audio.h"
#include "native2amiga_api.h"
#include "resampler.h"
#include "mhi.h"

#define MHI_DECODERS 4

/* the calls of mhiuae.library */
#define MHI_CMD_VERSION 0
#define MHI_CMD_ALLOC 1       /* task, signal mask */
#define MHI_CMD_FREE 2        /* handle */
#define MHI_CMD_QUEUE 3       /* handle, buffer, size */
#define MHI_CMD_GETEMPTY 4    /* handle */
#define MHI_CMD_STATUS 5      /* handle */
#define MHI_CMD_PLAY 6        /* handle */
#define MHI_CMD_STOP 7        /* handle */
#define MHI_CMD_PAUSE 8       /* handle */
#define MHI_CMD_SETPARAM 9    /* handle, parameter, value */

#define MHI_API_VERSION 1

/* libraries/mhi.h */
#define MHIF_PLAYING 0
#define MHIF_STOPPED 1
#define MHIF_OUT_OF_DATA 2
#define MHIF_PAUSED 3

#define MHIP_VOLUME 0
#define MHIP_PANNING 1
#define MHIP_CROSSMIXING 2

/* Decoded stereo frames waiting to be played. Topped up to PCM_LOW frames
   and more, by up to a frame (1152) at a time; a power of two, so indexes
   wrap by mask. */
#define PCM_FRAMES 8192
#define PCM_LOW 2048

/* How much of a buffer mpg123 gets at a time, when it needs more. */
#define FEED_CHUNK 4096

struct mhi_buffer {
	uaecptr addr;
	uae_u32 size;
	uae_u32 fed;
};

struct mhi_decoder {
	bool used;
	uaecptr task;
	uae_u32 sigmask;
	int status;

	/* the player's buffers: not all taken in yet, and handed back */
	std::deque<mhi_buffer> queue;
	std::deque<uaecptr> empty;

	mpg123_handle *mh;
	int freq;
	bool stream_error;

	uae_s16 pcm[PCM_FRAMES][2];
	unsigned int pcm_read, pcm_write;

	int volume, panning, crossmixing;
	float gain_ll, gain_lr, gain_rl, gain_rr;

	/* samples of silence in a row, up to RS_HIST: past that the resampler's
	   output is silence too, and the mixing is left out */
	int quiet;

	struct resampler rs;
};

static struct mhi_decoder decoders[MHI_DECODERS];
static bool mpg123_ready;

bool mhi_playing;

static bool audible(struct mhi_decoder *d)
{
	return d->used && d->status != MHIF_STOPPED;
}

static void update_playing(void)
{
	bool playing = false;
	for (int i = 0; i < MHI_DECODERS; i++) {
		if (audible(&decoders[i]))
			playing = true;
	}
	if (playing && !mhi_playing)
		audio_activate();
	mhi_playing = playing;
}

/* Volume and panning as the MAS driver has them, crossmixing up to mono. */
static void update_gains(struct mhi_decoder *d)
{
	int vl = d->volume, vr = d->volume;
	if (d->panning > 50)
		vl -= 2 * (d->panning - 50);
	if (d->panning < 50)
		vr -= 2 * (50 - d->panning);
	vl = vl < 0 ? 0 : (vl > 100 ? 100 : vl);
	vr = vr < 0 ? 0 : (vr > 100 ? 100 : vr);
	float cross = d->crossmixing / 200.0f;
	d->gain_ll = vl / 100.0f * (1.0f - cross);
	d->gain_lr = vl / 100.0f * cross;
	d->gain_rr = vr / 100.0f * (1.0f - cross);
	d->gain_rl = vr / 100.0f * cross;
}

static unsigned int pcm_fill(struct mhi_decoder *d)
{
	return d->pcm_write - d->pcm_read;
}

/* Starts mpg123 on a new stream: whatever it had of the old one goes. */
static bool stream_open(struct mhi_decoder *d)
{
	if (d->mh) {
		mpg123_close(d->mh);
	} else {
		if (!mpg123_ready) {
			if (mpg123_init() != MPG123_OK)
				return false;
			mpg123_ready = true;
		}
		d->mh = mpg123_new(NULL, NULL);
		if (!d->mh)
			return false;
		/* Always 16-bit stereo, at the stream's own rate: mono comes out on
		   both sides. */
		const long *rates;
		size_t count;
		mpg123_rates(&rates, &count);
		mpg123_format_none(d->mh);
		for (size_t i = 0; i < count; i++)
			mpg123_format(d->mh, rates[i], MPG123_STEREO, MPG123_ENC_SIGNED_16);
		mpg123_param(d->mh, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	}
	d->stream_error = false;
	d->pcm_read = d->pcm_write = 0;
	return mpg123_open_feed(d->mh) == MPG123_OK;
}

static void copy_from_amiga(uae_u8 *dst, uaecptr src, uae_u32 size)
{
	if (valid_address(src, size)) {
		memcpy(dst, get_real_address(src), size);
	} else {
		for (uae_u32 i = 0; i < size; i++)
			dst[i] = get_byte(src + i);
	}
}

/* Gives mpg123 the next piece of the queue, and hands the buffer back once
   it has had all of it. False when the queue is empty. */
static bool feed(struct mhi_decoder *d)
{
	if (d->queue.empty())
		return false;
	mhi_buffer &b = d->queue.front();
	uae_u8 chunk[FEED_CHUNK];
	uae_u32 n = b.size - b.fed;
	if (n > FEED_CHUNK)
		n = FEED_CHUNK;
	copy_from_amiga(chunk, b.addr + b.fed, n);
	mpg123_feed(d->mh, chunk, n);
	b.fed += n;
	if (b.fed >= b.size) {
		/* The player loops on MHIGetEmpty() after a signal, so one signal
		   for the buffers that pile up until then is enough. */
		bool signal = d->empty.empty();
		d->empty.push_back(b.addr);
		d->queue.pop_front();
		if (signal && d->task)
			uae_Signal(d->task, d->sigmask);
	}
	return true;
}

/* Decodes until PCM_LOW frames wait to be played, or the queue runs dry. */
static void decode(struct mhi_decoder *d)
{
	/* calls that bring neither sound nor take data; a few in a row are
	   normal, at the start of a stream */
	int idle = 0;
	while (pcm_fill(d) < PCM_LOW && idle < 16) {
		idle++;
		/* a frame of layer 3 at most; mpg123_read() rather than
		   mpg123_decode_frame(), whose off_t differs between builds */
		uae_s16 audio[1152 * 2];
		size_t bytes = 0;
		int ret = mpg123_read(d->mh, (unsigned char *)audio, sizeof audio, &bytes);
		if (ret == MPG123_NEW_FORMAT) {
			long rate;
			int channels, encoding;
			mpg123_getformat(d->mh, &rate, &channels, &encoding);
			write_log(_T("MHI: %ld Hz stream\n"), rate);
			if (rate != d->freq) {
				d->freq = (int)rate;
				resampler_rate(&d->rs, d->freq);
			}
			continue;
		}
		if (bytes > 0) {
			idle = 0;
			for (size_t i = 0; i < bytes / 4; i++) {
				unsigned int w = d->pcm_write++ & (PCM_FRAMES - 1);
				d->pcm[w][0] = audio[i * 2];
				d->pcm[w][1] = audio[i * 2 + 1];
			}
		}
		if (ret == MPG123_NEED_MORE) {
			if (!feed(d))
				break;
			idle = 0;
		} else if (ret != MPG123_OK) {
			/* mpg123 finds the next frame by itself; say so only once */
			if (!d->stream_error)
				write_log(_T("MHI: %s\n"), mpg123_strerror(d->mh));
			d->stream_error = true;
			if (!feed(d))
				break;
		}
	}
}

/* One sample of the decoder's clock. */
static void decoder_tick(struct mhi_decoder *d)
{
	float l = 0, r = 0;
	if (d->status == MHIF_PLAYING || d->status == MHIF_OUT_OF_DATA) {
		/* run dry: only new buffers bring more */
		if (pcm_fill(d) == 0 && !(d->status == MHIF_OUT_OF_DATA && d->queue.empty()))
			decode(d);
		if (pcm_fill(d) > 0) {
			unsigned int i = d->pcm_read++ & (PCM_FRAMES - 1);
			float sl = d->pcm[i][0], sr = d->pcm[i][1];
			l = sl * d->gain_ll + sr * d->gain_lr;
			r = sr * d->gain_rr + sl * d->gain_rl;
			d->status = MHIF_PLAYING;
		} else if (d->status == MHIF_PLAYING) {
			/* The player may wait for this at the end of a stream, after
			   the last buffer came back, as with the MAS the two come
			   together. */
			d->status = MHIF_OUT_OF_DATA;
			if (d->task)
				uae_Signal(d->task, d->sigmask);
		}
	}
	if (l == 0 && r == 0) {
		if (d->quiet < RS_HIST)
			d->quiet++;
	} else {
		d->quiet = 0;
	}
	resampler_put(&d->rs, l, r);
}

/* Runs the decoder up to the current emulated time. */
static void decoder_catchup(struct mhi_decoder *d)
{
	int n = resampler_catchup(&d->rs, audible(d));
	while (n-- > 0)
		decoder_tick(d);
}

static void mhi_hsync(void)
{
	bool sounding = false;
	for (int i = 0; i < MHI_DECODERS; i++) {
		struct mhi_decoder *d = &decoders[i];
		if (!d->used)
			continue;
		decoder_catchup(d);
		/* topped up here, so that decoding seldom falls on a sample */
		if (d->status == MHIF_PLAYING)
			decode(d);
		if (audible(d) && d->quiet < RS_HIST)
			sounding = true;
	}
	/* keeps Paula's output running, which the decoder's samples ride on */
	if (sounding)
		audio_activate();
}

static void mix(int *l, int *r)
{
	*l = *r = 0;
	for (int i = 0; i < MHI_DECODERS; i++) {
		struct mhi_decoder *d = &decoders[i];
		if (!audible(d))
			continue;
		decoder_catchup(d);
		if (d->quiet >= RS_HIST)
			continue;
		int dl, dr;
		resampler_output(&d->rs, &dl, &dr);
		*l += dl;
		*r += dr;
	}
}

void mhi_mix_stereo(uae_u32 *left, uae_u32 *right)
{
	int l, r;
	mix(&l, &r);
	*left = (uae_u16)clamp16((uae_s16)*left + l);
	*right = (uae_u16)clamp16((uae_s16)*right + r);
}

void mhi_mix_mono(uae_u32 *mono)
{
	int l, r;
	mix(&l, &r);
	*mono = (uae_u16)clamp16((uae_s16)*mono + (l + r) / 2);
}

/* MHIStop(): the queue is dropped, the buffers in it stay the player's. */
static void decoder_stop(struct mhi_decoder *d)
{
	d->status = MHIF_STOPPED;
	d->queue.clear();
	d->empty.clear();
	stream_open(d);
	resampler_clear(&d->rs);
	d->quiet = RS_HIST;
}

static void decoder_free(struct mhi_decoder *d)
{
	if (!d->used)
		return;
	decoder_stop(d);
	if (d->mh) {
		mpg123_delete(d->mh);
		d->mh = NULL;
	}
	d->used = false;
	d->task = 0;
}

static struct mhi_decoder *get_decoder(uae_u32 handle)
{
	if (handle < 1 || handle > MHI_DECODERS || !decoders[handle - 1].used)
		return NULL;
	return &decoders[handle - 1];
}

static uae_u32 decoder_alloc(uaecptr task, uae_u32 sigmask)
{
	for (int i = 0; i < MHI_DECODERS; i++) {
		struct mhi_decoder *d = &decoders[i];
		if (d->used)
			continue;
		if (!stream_open(d)) {
			write_log(_T("MHI: mpg123 does not start\n"));
			return 0;
		}
		d->used = true;
		d->task = task;
		d->sigmask = sigmask;
		d->status = MHIF_STOPPED;
		d->volume = 100;
		d->panning = 50;
		d->crossmixing = 0;
		update_gains(d);
		d->freq = 44100;
		resampler_start(&d->rs, d->freq);
		d->quiet = RS_HIST;
		write_log(_T("MHI: decoder %d for task %08x, signals %08x\n"), i + 1, task, sigmask);
		return i + 1;
	}
	return 0;
}

uae_u32 mhi_call(uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3)
{
	if (cmd == MHI_CMD_VERSION)
		return MHI_API_VERSION;
	if (cmd == MHI_CMD_ALLOC)
		return decoder_alloc(arg1, arg2);

	struct mhi_decoder *d = get_decoder(arg1);
	if (!d)
		return 0;
	/* everything up to now as it was before the call */
	decoder_catchup(d);
	uae_u32 ret = 0;
	switch (cmd) {
	case MHI_CMD_FREE:
		decoder_free(d);
		break;
	case MHI_CMD_QUEUE:
		if (!arg2 || !arg3)
			break;
		d->queue.push_back({ arg2, arg3, 0 });
		ret = 0xffffffff;
		break;
	case MHI_CMD_GETEMPTY:
		if (!d->empty.empty()) {
			ret = d->empty.front();
			d->empty.pop_front();
		}
		break;
	case MHI_CMD_STATUS:
		ret = d->status;
		break;
	case MHI_CMD_PLAY:
		if (d->status == MHIF_STOPPED)
			resampler_start(&d->rs, d->freq);
		if (d->status != MHIF_PLAYING) {
			d->status = MHIF_PLAYING;
			decode(d);
		}
		break;
	case MHI_CMD_STOP:
		decoder_stop(d);
		break;
	case MHI_CMD_PAUSE:
		/* the MAS driver's way: a second pause plays on */
		if (d->status == MHIF_PAUSED)
			d->status = MHIF_PLAYING;
		else if (d->status == MHIF_PLAYING || d->status == MHIF_OUT_OF_DATA)
			d->status = MHIF_PAUSED;
		break;
	case MHI_CMD_SETPARAM:
		if (arg3 > 100)
			arg3 = 100;
		switch (arg2) {
		case MHIP_VOLUME:
			d->volume = arg3;
			break;
		case MHIP_PANNING:
			d->panning = arg3;
			break;
		case MHIP_CROSSMIXING:
			d->crossmixing = arg3;
			break;
		}
		update_gains(d);
		break;
	}
	update_playing();
	return ret;
}

static void mhi_reset(int hardreset)
{
	/* the players and their buffers are gone */
	for (int i = 0; i < MHI_DECODERS; i++)
		decoder_free(&decoders[i]);
	update_playing();
}

static void mhi_free(void)
{
	mhi_reset(1);
}

void mhi_install(void)
{
	device_add_hsync(mhi_hsync);
	device_add_reset(mhi_reset);
	device_add_exit(mhi_free);
}
