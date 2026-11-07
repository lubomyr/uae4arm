#ifndef UAE_HOSTVIDEO_H
#define UAE_HOSTVIDEO_H

/* Video decoded by the host and shown in a window of the Amiga, through the
   HostPlay tool (amiga/HostPlay.s). */

#include "uae/types.h"

struct TrapContext;

/* True while a video with sound plays, so the per-sample mixing (sound.h)
   costs one test otherwise. */
extern bool hostvideo_playing;
extern void hostvideo_mix_stereo(uae_u32 *left, uae_u32 *right);
extern void hostvideo_mix_mono(uae_u32 *mono);

extern void hostvideo_install(void);
/* uaelib function 202 */
/* the shape a screen of w x h pixels is seen in (pandora_gfx.cpp) */
extern void rtg_shown_shape(int w, int h, int *shown_w, int *shown_h);
extern uae_u32 hostvideo_call(struct TrapContext *ctx, uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3, uae_u32 arg4);

#endif /* UAE_HOSTVIDEO_H */
