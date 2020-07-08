#include "sysdeps.h"

#include <ctype.h>
#include <assert.h>

#include "options.h"
#include "xwin.h"
#include "autoconf.h"
#include "gui.h"
#include "custom.h"
#include "drawing.h"
#include "statusline.h"

static const char *numbers = { /* ugly  0123456789CHD%+-PNK */
	"+++++++--++++-+++++++++++++++++-++++++++++++++++++++++++++++++++++++++++++++-++++++-++++----++---+--------------+++++++++++++++++++++"
	"+xxxxx+--+xx+-+xxxxx++xxxxx++x+-+x++xxxxx++xxxxx++xxxxx++xxxxx++xxxxx++xxxx+-+x++x+-+xxx++-+xx+-+x---+----------+xxxxx++x+++x++x++x++"
	"+x+++x+--++x+-+++++x++++++x++x+++x++x++++++x++++++++++x++x+++x++x+++x++x++++-+x++x+-+x++x+--+x++x+--+x+----+++--+x---x++xx++x++x+x+++"
	"+x+-+x+---+x+-+xxxxx++xxxxx++xxxxx++xxxxx++xxxxx+--++x+-+xxxxx++xxxxx++x+----+xxxx+-+x++x+----+x+--+xxx+--+xxx+-+xxxxx++x+x+x++xx++++"
	"+x+++x+---+x+-+x++++++++++x++++++x++++++x++x+++x+--+x+--+x+++x++++++x++x++++-+x++x+-+x++x+---+x+x+--+x+----+++--+x++++++x+x+x++x+x+++"
	"+xxxxx+---+x+-+xxxxx++xxxxx+----+x++xxxxx++xxxxx+--+x+--+xxxxx++xxxxx++xxxx+-+x++x+-+xxx+---+x++xx--------------+x+----+x++xx++x++x++"
	"+++++++---+++-++++++++++++++----+++++++++++++++++--+++--++++++++++++++++++++-++++++-++++------------------------+++----++++++++++++++"
};

static void putpixel (uae_u8 *buf, int bpp, int x, xcolnr c8)
{
  if (x <= 0)
    return;

  switch (bpp) {
    case 2:
    {
	    uae_u16 *p = (uae_u16 *)buf + x;
	    *p = (uae_u16)c8;
      break;
    }
    case 4:
    {
      uae_u32 *p = (uae_u32*)buf + x;
      *p = c8;
      break;
    }
  }
}

STATIC_INLINE uae_u32 ledcolor(uae_u32 c, uae_u32 *rc, uae_u32 *gc, uae_u32 *bc)
{
	uae_u32 v = rc[(c >> 16) & 0xff] | gc[(c >> 8) & 0xff] | bc[(c >> 0) & 0xff];
	return v;
}

static void write_tdnumber (uae_u8 *buf, int bpp, int x, int y, int num, uae_u32 c1, uae_u32 c2)
{
  int j;
  const char *numptr;

  numptr = numbers + num * TD_NUM_WIDTH + NUMBERS_NUM * TD_NUM_WIDTH * y;
  for (j = 0; j < TD_NUM_WIDTH; j++) {
  	if (*numptr == 'x')
      putpixel (buf, bpp, x + j, c1);
    else if (*numptr == '+')
      putpixel (buf, bpp, x + j, c2);
	  numptr++;
  }
}

void draw_status_line_single (uae_u8 *buf, int bpp, int y, int totalwidth, uae_u32 *rc, uae_u32 *gc, uae_u32 *bc)
{
	struct amigadisplay *ad = &adisplays;
  uae_u32 ledmask = currprefs.leds_on_screen_mask[ad->picasso_on ? 1 : 0];
  int x_start, j, led, border;
  uae_u32 c1, c2, cb;

	c1 = ledcolor (0x00ffffff, rc, gc, bc);
	c2 = ledcolor (0x00000000, rc, gc, bc);
	cb = ledcolor (TD_BORDER, rc, gc, bc);

  ledmask &= ~(1 << LED_POWER);
  if(nr_units() < 1)
    ledmask &= ~(1 << LED_HD);
  for(j = 0; j <= 3; ++j) {
    if(j > currprefs.nr_floppies - 1)
      ledmask &= ~(1 << (LED_DF0 + j));
  }
  if(!currprefs.cdslots[0].inuse)
    ledmask &= ~(1 << LED_CD);

  x_start = totalwidth - TD_PADX - VISIBLE_LEDS * TD_WIDTH;
  for(led = 0; led < LED_MAX; ++led) {
    if(!(ledmask & (1 << led)))
      x_start += TD_WIDTH;
  }

	for (led = LED_MAX - 1; led >= 0; --led) {
		int num1 = -1, num2 = -1, num3 = -1;
		int x, c, on = 0, am = 2;
    int on_rgb = 0, on_rgb2 = 0, off_rgb = 0, pen_rgb = 0;
		int half = 0;

    if(!(ledmask & (1 << led)))
      continue;

		pen_rgb = c1;
		if (led >= LED_DF0 && led <= LED_DF3) {
			int pled = currprefs.nr_floppies - (led - LED_DF0) - 1;
			int track = gui_data.drive_track[pled];
			on_rgb = 0x00cc00;
			on_rgb2 = 0x006600;
			off_rgb = 0x003300;
			num1 = -1;
			num2 = track / 10;
			num3 = track % 10;
			on = gui_data.drive_motor[pled];
      if (gui_data.drive_writing[pled]) {
		    on_rgb = 0xcc0000;
				on_rgb2 = 0x880000;
      }
			half = gui_data.drive_side ? 1 : -1;
			if (gui_data.df[pled][0] == 0)
				pen_rgb = ledcolor (0x00aaaaaa, rc, gc, bc);
		} else if (led == LED_CD) {
			if (gui_data.cd >= 0) {
				on = gui_data.cd & (LED_CD_AUDIO | LED_CD_ACTIVE);
				on_rgb = (on & LED_CD_AUDIO) ? 0x00cc00 : 0x0000cc;
				if ((gui_data.cd & LED_CD_ACTIVE2) && !(gui_data.cd & LED_CD_AUDIO)) {
					on_rgb &= 0xfefefe;
					on_rgb >>= 1;
				}
				off_rgb = 0x000033;
				num1 = -1;
				num2 = 10;
				num3 = 12;
			}
		} else if (led == LED_HD) {
			if (gui_data.hd >= 0) {
				on = gui_data.hd;
        on_rgb = on == 2 ? 0xcc0000 : 0x0000cc;
        off_rgb = 0x000033;
			  num1 = -1;
			  num2 = 11;
			  num3 = 12;
			}
		} else if (led == LED_FPS) {
			int fps = (gui_data.fps + 5) / 10;
			on = gui_data.powerled;
			on_rgb = 0xcc0000;
			off_rgb = 0x330000;
			am = 3;
			num1 = fps / 100;
			num2 = (fps - num1 * 100) / 10;
			num3 = fps % 10;
			if (num1 == 0)
				am = 2;
		} else if (led == LED_CPU) {
			int idle = ((1000 - gui_data.idle) + 5) / 10;
			if(idle < 0)
			  idle = 0;
			on_rgb = 0x666666;
			off_rgb = 0x666666;
			if (gui_data.cpu_halted) {
			  idle = 0;
			  on = 1;
				on_rgb = 0xcccc00;
				num1 = gui_data.cpu_halted >= 10 ? 11 : -1;
				num2 = gui_data.cpu_halted >= 10 ? gui_data.cpu_halted / 10 : 11;
				num3 = gui_data.cpu_halted % 10;
				am = 2;
			} else {
			  num1 = idle / 100;
			  num2 = (idle - num1 * 100) / 10;
			  num3 = idle % 10;
				am = num1 > 0 ? 3 : 2;
      }
		} else {
			continue;
		}
		if (half > 0) {
			c = ledcolor(on ? (y >= TD_TOTAL_HEIGHT / 2 ? on_rgb2 : on_rgb) : off_rgb, rc, gc, bc);
		} else if (half < 0) {
			c = ledcolor(on ? (y < TD_TOTAL_HEIGHT / 2 ? on_rgb2 : on_rgb) : off_rgb, rc, gc, bc);
		} else {
			c = ledcolor(on ? on_rgb : off_rgb, rc, gc, bc);
		}
		border = 0;
		if (y == 0 || y == TD_TOTAL_HEIGHT - 1) {
			c = ledcolor (TD_BORDER, rc, gc, bc);
			border = 1;
		}

    x = x_start;
		if (!border)
			putpixel (buf, bpp, x - 1, cb);
    for (j = 0; j < TD_LED_WIDTH; j++) 
	    putpixel (buf, bpp, x + j, c);
		if (!border)
			putpixel (buf, bpp, x + j, cb);

	  if (y >= TD_PADY && y - TD_PADY < TD_NUM_HEIGHT) {
			if (num3 >= 0) {
				x += (TD_LED_WIDTH - am * TD_NUM_WIDTH) / 2;
        if(num1 > 0) {
        	write_tdnumber (buf, bpp, x, y - TD_PADY, num1, pen_rgb, c2);
        	x += TD_NUM_WIDTH;
        }
				if (num2 >= 0) {
          write_tdnumber (buf, bpp, x, y - TD_PADY, num2, pen_rgb, c2);
        	x += TD_NUM_WIDTH;
        }
        write_tdnumber (buf, bpp, x, y - TD_PADY, num3, pen_rgb, c2);
				x += TD_NUM_WIDTH;
      }
	  }
	  x_start += TD_WIDTH;
  }
}
