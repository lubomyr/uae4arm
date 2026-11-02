/*
* UAE - The Un*x Amiga Emulator
*
* Resampling of the sound sources that keep a clock of their own (resampler.h)
*
* Taken out of the Toccata's emulation (sndboard.cpp), which had it first.
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include <math.h>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "events.h"
#include "resampler.h"

/* Colour clocks per second of a PAL Amiga, for when sound emulation is off and
   update_sound() never tells us the real figure: the Toccata's driver still
   waits on the FIFO, so it has to drain. */
#define DEFAULT_EVENT_CLOCK 3546895.0

static double base_event_clock;

void update_resampler_clock(double clk)
{
	base_event_clock = clk;
}

double resampler_clock(void)
{
	return base_event_clock > 0 ? base_event_clock : DEFAULT_EVENT_CLOCK;
}

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

/* Fills the resampler's table: for each phase between two source samples, the
   weights of the RS_TAPS samples around the output instant. The cutoff is a
   fraction of the source's rate; each row is scaled to sum to 1, so that
   silence with an offset stays silent whatever the phase. */
static void resampler_setup(struct resampler *rs, double cutoff)
{
	if (cutoff == rs->cutoff)
		return;
	rs->cutoff = cutoff;
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
			rs->coef[p][j] = (float)(c[j] / sum);
	}
}

void resampler_clear(struct resampler *rs)
{
	memset(rs->hist, 0, sizeof rs->hist);
	rs->hist_pos = 0;
}

void resampler_rate(struct resampler *rs, int freq)
{
	rs->event_time = (int)(resampler_clock() * CYCLE_UNIT / freq);
	if (rs->cycle_acc >= rs->event_time)
		rs->cycle_acc = 0;

	/* Up to Nyquist of the source, or of the output if that is lower, so
	   that nothing above it folds back into the audible range. */
	double cutoff = RS_CUTOFF;
	if (currprefs.sound_freq > 0 && currprefs.sound_freq < freq)
		cutoff = RS_CUTOFF * currprefs.sound_freq / freq;
	resampler_setup(rs, cutoff);
}

void resampler_start(struct resampler *rs, int freq)
{
	rs->last_cycles = get_cycles();
	rs->cycle_acc = 0;
	resampler_rate(rs, freq);
	resampler_clear(rs);
}

int resampler_catchup(struct resampler *rs, bool running)
{
	uae_u32 now = get_cycles();
	uae_u32 delta = now - rs->last_cycles;
	rs->last_cycles = now;
	if (!running || rs->event_time <= 0)
		return 0;
	/* after a pause or a long stall, do not replay seconds of samples at once */
	uae_u32 limit = (uae_u32)rs->event_time * 4096;
	if (delta > limit)
		delta = limit;
	rs->cycle_acc += delta;
	int n = (int)(rs->cycle_acc / rs->event_time);
	rs->cycle_acc -= (uae_s64)n * rs->event_time;
	return n;
}

/* Delayed by a fixed RS_TAPS / 2 source samples. The phase falls between two
   rows of the table, so both are applied and the results interpolated. */
void resampler_output(struct resampler *rs, int *l, int *r)
{
	if (rs->event_time <= 0) {
		*l = *r = 0;
		return;
	}
	/* where the instant lies from the source's latest sample, in samples:
	   below 1, and below 0 when that sample came after it */
	uae_u32 latest = rs->last_cycles - (uae_u32)rs->cycle_acc;
	float since = (float)(uae_s32)(audio_sample_cycles - latest) / rs->event_time;
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
	const float *c0 = rs->coef[p], *c1 = rs->coef[p + 1];
	unsigned int base = rs->hist_pos - RS_TAPS - RS_LOOKBACK + whole;
	float l0 = 0, l1 = 0, r0 = 0, r1 = 0;
	for (int j = 0; j < RS_TAPS; j++) {
		unsigned int i = (base + j) & (RS_HIST - 1);
		float sl = rs->hist[0][i], sr = rs->hist[1][i];
		l0 += c0[j] * sl;
		l1 += c1[j] * sl;
		r0 += c0[j] * sr;
		r1 += c1[j] * sr;
	}
	*l = (int)lrintf(l0 + (l1 - l0) * f);
	*r = (int)lrintf(r0 + (r1 - r0) * f);
}
