/*
 * uae4arm - paravirtual AHI sound
 *
 * The host side of the protocol spoken by WinUAE's uae.audio AHI driver.
 */

#ifndef UAE4ARM_AHI_V1_H
#define UAE4ARM_AHI_V1_H

#ifdef AHI
#include "traps.h"

extern uae_u32 REGPARAM2 ahi_demux (TrapContext *ctx);
extern void ahi_init (void);
/* Adds whatever AHI has queued into a block the SDL mixer is about to play. */
extern void ahi_mix (uae_s16 *stream, int frames, bool stereo);
#endif

#endif /* UAE4ARM_AHI_V1_H */
