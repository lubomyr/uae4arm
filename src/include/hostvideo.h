#ifndef UAE_HOSTVIDEO_H
#define UAE_HOSTVIDEO_H

/* Video decoded by the host and shown in a window of the Amiga, through the
   HostPlay tool (amiga/HostPlay.s). */

#include "uae/types.h"

struct TrapContext;

extern void hostvideo_install(void);
/* uaelib function 202 */
extern uae_u32 hostvideo_call(struct TrapContext *ctx, uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3, uae_u32 arg4);

#endif /* UAE_HOSTVIDEO_H */
