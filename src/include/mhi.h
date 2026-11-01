#ifndef UAE_MHI_H
#define UAE_MHI_H

/* MPEG audio decoding on the host for MHI, the interface of the MPEG audio
   decoder cards, through the mhiuae.library driver (amiga/mhiuae.s). */

#include "uae/types.h"

/* True while a decoder plays, so the per-sample mixing (sound.h) costs one
   test otherwise. */
extern bool mhi_playing;
extern void mhi_mix_stereo(uae_u32 *left, uae_u32 *right);
extern void mhi_mix_mono(uae_u32 *mono);

extern void mhi_install(void);
/* uaelib function 201 */
extern uae_u32 mhi_call(uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3);

#endif /* UAE_MHI_H */
