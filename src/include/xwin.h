 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Interface to the graphics system (X, SVGAlib)
  *
  * Copyright 1995-1997 Bernd Schmidt
  */

#ifndef UAE_XWIN_H
#define UAE_XWIN_H

#include "uae/types.h"
#include "machdep/rpt.h"

typedef uae_u32 xcolnr;

typedef int (*allocfunc_type)(int, int, int, xcolnr *);

extern xcolnr xcolors[4096];

extern int graphics_setup (void);
extern int graphics_init (bool);
extern void graphics_leave (void);
extern int handle_msgpump (void);

extern bool vsync_switchmode (int hz);
extern bool render_screen (bool);
extern void show_screen (int mode);

extern int lockscr (void);
extern void unlockscr (void);
extern bool target_graphics_buffer_update (void);

extern void screenshot (int);

extern int bits_in_mask (unsigned long mask);
extern int mask_shift (unsigned long mask);
extern unsigned int doMask256 (int p, int bits, int shift);
extern void alloc_colors64k (int, int, int, int, int, int);
extern void alloc_colors_picasso (int rw, int gw, int bw, int rs, int gs, int bs, int rgbfmt);

struct vidbuffer
{
  uae_u8 *bufmem;
  int rowbytes; /* Bytes per row in the memory pointed at by bufmem. */
  int pixbytes; /* Bytes per pixel. */
	/* size of max visible image */
  int outwidth;
  int outheight;
};

extern int max_uae_width, max_uae_height;

struct vidbuf_description
{
  struct vidbuffer drawbuffer;
};

struct amigadisplay
{
	volatile bool picasso_requested_on;
	bool picasso_requested_forced_on;
	bool picasso_on;
	int framecnt;
	int inhibit_frame;

	struct vidbuf_description gfxvidinfo;
};

extern struct amigadisplay adisplays;

STATIC_INLINE int isvsync_chipset (void)
{
	struct amigadisplay *ad = &adisplays;
	if (ad->picasso_on)
		return 0;
	return 1;
}

#endif /* UAE_XWIN_H */
