#ifndef UAE_RESAMPLER_H
#define UAE_RESAMPLER_H

/*
* A sound source with a clock of its own in emulated time - the Toccata's
* codec, the MHI decoder - whose samples ride on Paula's: every sample Paula
* puts out gets the source's output at that instant added to it (sound.h).
* Both then share the same buffer, the same pacing and the same rate control.
*/

#include "uae/types.h"

/* The source's samples are resampled to the instants Paula puts hers out with
   a windowed sinc. Paula's rate is never exactly the source's - the sound rate
   control moves it by up to 0.4% - so those instants slide past the source's
   samples. Linear interpolation dulls the high frequencies the more, the
   nearer an instant falls to the middle between two samples, and the sliding
   turned that into a hiss on cymbals and the like. The sinc's response stays
   flat to about 20 kHz wherever the instant falls. */
#define RS_TAPS 96
#define RS_PHASES 256              /* table steps between two source samples */
#define RS_KAISER_BETA 7.86        /* about 80 dB of stopband */
/* The pass band ends short of Nyquist, so that the stop band starts at it:
   centred on Nyquist, the images of what lies just below - plenty in some
   mp3s - came through. Flat to about 20 kHz at 44.1 kHz. */
#define RS_CUTOFF 0.477
/* Paula's samples come in a batch at the end of each stretch update_audio()
   works through - about one per scanline - by which time the source has run
   on. Taking the source's output as of then gave every sample of the batch
   the same value, a staircase at the scanline rate that folded everything
   above about 8 kHz back down as noise. The history keeps this many source
   samples more, so the output can be worked out for the instant of each
   sample even when it lies that far before the source's latest one. */
#define RS_LOOKBACK 16
#define RS_HIST 256                /* power of two, at least RS_TAPS + RS_LOOKBACK */

struct resampler {
	/* the source's recent samples, volume applied */
	float hist[2][RS_HIST];
	unsigned int hist_pos;

	/* the source's clock */
	int event_time;
	uae_u32 last_cycles;
	uae_s64 cycle_acc;

	float coef[RS_PHASES + 1][RS_TAPS];
	double cutoff;
};

/* Sets the clock to the source's rate and clears the history. */
extern void resampler_start(struct resampler *rs, int freq);
/* Changes the rate of a source that plays on. */
extern void resampler_rate(struct resampler *rs, int freq);
extern void resampler_clear(struct resampler *rs);
/* How many samples the source owes up to the current emulated time; the
   clock moves on by as many. A source that is not running owes none, but its
   clock keeps up. */
extern int resampler_catchup(struct resampler *rs, bool running);
/* The source's output at the instant of Paula's sample. */
extern void resampler_output(struct resampler *rs, int *l, int *r);

static inline void resampler_put(struct resampler *rs, float l, float r)
{
	unsigned int i = rs->hist_pos & (RS_HIST - 1);
	rs->hist[0][i] = l;
	rs->hist[1][i] = r;
	rs->hist_pos++;
}

static inline int clamp16(int v)
{
	return v > 32767 ? 32767 : (v < -32768 ? -32768 : v);
}

/* The emulated time of the sample Paula is putting out. update_audio() works
   through a stretch of time at once, so get_cycles() is the end of it, not
   the instant of the sample. */
extern uae_u32 audio_sample_cycles;

/* Colour clocks per second, from update_sound(). */
extern void update_resampler_clock(double clk);
/* The same, for the other things that keep time with the sound. */
extern double resampler_clock(void);

#endif /* UAE_RESAMPLER_H */
