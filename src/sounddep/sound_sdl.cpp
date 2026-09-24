 /* 
  * Sdl sound.c implementation
  * (c) 2015
  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#include "audio.h"
#include "driveclick.h"
#include "gensound.h"
#include "sounddep/sound.h"
#include <SDL.h>

#ifdef ANDROIDSDL
#include <android/log.h>
#endif

// "consumer" means the actual SDL sound output, as opposed to 
#define SOUND_CONSUMER_BUFFER_LENGTH SNDBUFFER_LEN

#ifdef AHI
#include "ahi_v1.h"
#endif

uae_u16 sndbuffer[SOUND_BUFFERS_COUNT][(SNDBUFFER_LEN + 32) * DEFAULT_SOUND_CHANNELS];
uae_u16 *sndbufpt = sndbuffer[0];
uae_u16 *render_sndbuff = sndbuffer[0];
uae_u16 *finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN * 2;

uae_u16 cdaudio_buffer[CDAUDIO_BUFFERS][(CDAUDIO_BUFFER_LEN + 32) * 2];
uae_u16 *cdbufpt = cdaudio_buffer[0];
uae_u16 *render_cdbuff = cdaudio_buffer[0];
uae_u16 *finish_cdbuff = cdaudio_buffer[0] + CDAUDIO_BUFFER_LEN * 2;
bool cdaudio_active = false;
static int cdwrcnt = 0;
static int cdrdcnt = 0;


static int have_sound = 0;
static float scaled_sample_evtime_orig;
static float snd_adjust = 1.0f;

void update_sound(double clk)
{
	double evtime;
  
	evtime = clk * CYCLE_UNIT / (double)currprefs.sound_freq;
	scaled_sample_evtime_orig = evtime;
	scaled_sample_evtime = scaled_sample_evtime_orig * snd_adjust;
}

void sound_adjust(float factor)
{
  snd_adjust = factor;
  scaled_sample_evtime = scaled_sample_evtime_orig * snd_adjust;
}


static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;
static int sound_thread_active = 0, sound_thread_exit = 0;
static int rdcnt = 0;
static int snd_rdpos = 0; /* bytes already taken out of the buffer at rdcnt */
static int wrcnt = 0;
static const int cnt_max_diff = 8;

/* The emulator's sample clock and the sound device's are never exactly equal, so
   the gap between them creeps up until the catch-up below throws away two whole
   buffers - an audible crack every half minute or so. Nudging the emulated sample
   rate by a fraction of a percent holds the gap steady instead, which is what the
   Raspberry Pi targets do with sound_adjust() for their display rate. */
static void adjust_sound_rate(void)
{
	static float fill_avg = -1.0f;
	static float creep;          /* the standing difference between the two clocks */
	static int cnt;
	const float target = 4.0f;   /* buffers kept between the emulator and the device */
	const float limit = 0.004f;  /* at most 0.4% off the nominal rate */

	float fill = (float)(wrcnt - rdcnt);
	if (fill_avg < 0.0f)
		fill_avg = fill;
	/* The device asks for its data unevenly, so the level swings by a buffer or two
	   on its own - average long enough that the swings are not chased. */
	fill_avg += (fill - fill_avg) * 0.005f;

	if (++cnt < 44) /* about once every two seconds */
		return;
	cnt = 0;

	float err = fill_avg - target;

	/* The level is the running total of the rate difference, so correcting by the
	   level alone leaves the loop free to swing. The bulk of the correction follows
	   the level directly, which damps it; the slow part below settles the standing
	   difference between the clocks. Both stay far too small to be heard. */
	creep += err > 0.0f ? 0.00004f : -0.00004f;
	if (creep < -limit)
		creep = -limit;
	if (creep > limit)
		creep = limit;

	float adj = 1.0f + err * 0.0004f + creep;
	if (adj < 1.0f - limit)
		adj = 1.0f - limit;
	if (adj > 1.0f + limit)
		adj = 1.0f + limit;

	sound_adjust(adj);
}


static void sound_copy_produced_block(void *ud, Uint8 *stream, int len)
{
	/* The device's chunk size need not match the emulator's buffer, so carry on
	   from wherever inside the current buffer the last chunk stopped. */
	const int blocksize = (currprefs.sound_stereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN) * 2;
	int done = 0;

	while (done < len) {
		if (wrcnt - rdcnt < 1) {
			memset(stream + done, 0, len - done);
			break;
		}

		if (snd_rdpos == 0 && currprefs.sound_stereo &&
			cdaudio_active && currprefs.sound_freq == 44100 && cdrdcnt < cdwrcnt)
		{
			for (int i = 0; i < SNDBUFFER_LEN * 2; ++i)
				sndbuffer[rdcnt & (SOUND_BUFFERS_COUNT - 1)][i] += cdaudio_buffer[cdrdcnt & (CDAUDIO_BUFFERS - 1)][i];
			cdrdcnt++;
		}

		int chunk = blocksize - snd_rdpos;
		if (chunk > len - done)
			chunk = len - done;
		memcpy(stream + done, (Uint8 *)sndbuffer[rdcnt & (SOUND_BUFFERS_COUNT - 1)] + snd_rdpos, chunk);
		done += chunk;
		snd_rdpos += chunk;

		if (snd_rdpos >= blocksize) {
			snd_rdpos = 0;
			rdcnt++;
		}
	}

#ifdef AHI
	/* Mixed in after Paula's block has been placed, so AHI still plays while
	   Paula has nothing to say and her buffer comes out as silence. */
	int fs = currprefs.sound_stereo ? 4 : 2;
	ahi_mix((uae_s16 *)stream, len / fs, currprefs.sound_stereo != 0,
		(uae_u32)rdcnt * SNDBUFFER_LEN + snd_rdpos / fs);
#endif
}


static void sound_thread_mixer(void *ud, Uint8 *stream, int len)
{
	if (sound_thread_exit) 
	  return;
	sound_thread_active = 1;

	int sample_size = currprefs.sound_stereo ? 4 : 2;

	while (len > 0) {
		int l = len < SNDBUFFER_LEN * sample_size ? len : SNDBUFFER_LEN * sample_size;
		sound_copy_produced_block(ud, stream, l);
		stream += l;
		len -= l;
	}
}


static void init_soundbuffer_usage(void)
{
	sndbufpt = sndbuffer[0];
	render_sndbuff = sndbuffer[0];
	finish_sndbuff = sndbuffer[0] + SNDBUFFER_LEN * 2;
	rdcnt = 0;
	wrcnt = 0;
	snd_rdpos = 0;
  
	cdbufpt = cdaudio_buffer[0];
	render_cdbuff = cdaudio_buffer[0];
	finish_cdbuff = cdaudio_buffer[0] + CDAUDIO_BUFFER_LEN * 2;
	cdrdcnt = 0;
	cdwrcnt = 0;
}


static int start_sound(int rate, int bits, int stereo)
{
	int frag = 0, buffers, ret;
	unsigned int bsize;

  if(SDL_GetAudioStatus() == SDL_AUDIO_STOPPED) {
  	init_soundbuffer_usage();

    s_oldrate = 0;
    s_oldbits = 0;
    s_oldstereo = 0;

		sound_thread_exit = 0;
  }

	// if no settings change, we don't need to do anything
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo) 
		return 0;

	SDL_AudioSpec as;
	memset(&as, 0, sizeof(as));
  
	as.freq = rate;
	as.format = (bits == 8 ? AUDIO_S8 : AUDIO_S16);
	as.channels = (stereo ? 2 : 1);
	as.samples = SOUND_CONSUMER_BUFFER_LENGTH;
	as.callback = sound_thread_mixer;

	if (SDL_OpenAudio(&as, NULL))
		printf("Error when opening SDL audio !\n");
  
	s_oldrate = rate; 
	s_oldbits = bits; 
	s_oldstereo = stereo;

	clear_sound_buffers();
	clear_cdaudio_buffers();

	SDL_PauseAudio(0);

	return 0;
}


void stop_sound(void)
{
  if(sound_thread_exit == 0) {
  	SDL_PauseAudio(1);
  	sound_thread_exit = 1;
  	SDL_CloseAudio();
  }
}

void finish_sound_buffer(void)
{
	if (currprefs.turbo_emulation) {
		sndbufpt = render_sndbuff = sndbuffer[wrcnt & (SOUND_BUFFERS_COUNT - 1)];
		return;
	}
#ifdef DRIVESOUND
	driveclick_mix((uae_s16*)render_sndbuff, currprefs.sound_stereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN);
#endif

	wrcnt++;
	sndbufpt = render_sndbuff = sndbuffer[wrcnt & (SOUND_BUFFERS_COUNT - 1)];

	if (currprefs.sound_stereo)
		finish_sndbuff = sndbufpt + SNDBUFFER_LEN * 2;
	else
		finish_sndbuff = sndbufpt + SNDBUFFER_LEN;

	adjust_sound_rate();

	if (wrcnt - rdcnt > cnt_max_diff)
	{
	  // Audiodriver has big delay, skip two buffers
		rdcnt = wrcnt - (cnt_max_diff - 1);
	} 
}

#ifdef AHI
/* Frames Paula has made so far, counted on the same scale as the consumed
   count handed to ahi_mix(). Emulator thread only. */
uae_u32 sound_frames_produced(void)
{
	int ch = currprefs.sound_stereo ? 2 : 1;
	return (uae_u32)wrcnt * SNDBUFFER_LEN + (uae_u32)(sndbufpt - render_sndbuff) / ch;
}
#endif

void pause_sound_buffer(void)
{
	reset_sound();
}

void restart_sound_buffer(void)
{
	sndbufpt = render_sndbuff = sndbuffer[wrcnt & (SOUND_BUFFERS_COUNT - 1)];
	if (currprefs.sound_stereo)
		finish_sndbuff = sndbufpt + SNDBUFFER_LEN * 2;
	else
		finish_sndbuff = sndbufpt + SNDBUFFER_LEN;

	cdbufpt = render_cdbuff = cdaudio_buffer[cdwrcnt & (CDAUDIO_BUFFERS - 1)];
	finish_cdbuff = cdbufpt + CDAUDIO_BUFFER_LEN * 2;
}

void finish_cdaudio_buffer(void)
{
	cdwrcnt++;
	cdbufpt = render_cdbuff = cdaudio_buffer[cdwrcnt & (CDAUDIO_BUFFERS - 1)];
	finish_cdbuff = cdbufpt + CDAUDIO_BUFFER_LEN * 2;
	audio_activate();
}


bool cdaudio_catchup(void)
{
	while ((cdwrcnt > cdrdcnt + CDAUDIO_BUFFERS - 10) && (sound_thread_active != 0) && (quit_program == 0)) {
		sleep_millis(10);
	}
	return (sound_thread_active != 0);
}

/* Try to determine whether sound is available.  This is only for GUI purposes.  */
int setup_sound(void)
{
	if (start_sound(currprefs.sound_freq, 16, currprefs.sound_stereo) != 0)
		return 0;

	sound_available = 1;
	return 1;
}

static int open_sound(void)
{
  config_changed = 1;
	if (start_sound(currprefs.sound_freq, 16, currprefs.sound_stereo) != 0)
		return 0;

	have_sound = 1;
	sound_available = 1;

	if (currprefs.sound_stereo)
		sample_handler = sample16s_handler;
	else
		sample_handler = sample16_handler;

	driveclick_init ();

	return 1;
}

void close_sound(void)
{
  config_changed = 1;
	if (!have_sound)
		return;

	stop_sound();

	have_sound = 0;
}

void pause_sound(void)
{
	SDL_PauseAudio(1);
}

void resume_sound(void)
{
	SDL_PauseAudio(0);
}

void reset_sound(void)
{
	if (!have_sound)
		return;

	init_soundbuffer_usage();

	clear_sound_buffers();
	clear_cdaudio_buffers();
}

int init_sound (void)
{
	if (!sound_available)
		return 0;
	if (currprefs.produce_sound <= 1)
		return 0;
	if (have_sound)
		return 1;
	if (!open_sound ())
		return 0;
	driveclick_reset ();
	return 1;
}

void sound_volume(int dir)
{
  config_changed = 1;
}
