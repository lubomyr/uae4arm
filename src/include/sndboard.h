#ifndef UAE_SNDBOARD_H
#define UAE_SNDBOARD_H

#ifdef TOCCATA

#include "uae/types.h"

extern bool toccata_init(struct autoconfig_info *aci);
extern void update_sndboard_sound(double clk);

/* True while the codec plays, so the per-sample mixing below costs one test
   when the board is idle or absent. */
extern bool sndboard_playing;
extern void sndboard_mix_stereo(uae_u32 *left, uae_u32 *right);
extern void sndboard_mix_mono(uae_u32 *data);

#endif

#endif /* UAE_SNDBOARD_H */
