#include "sysdeps.h"

#include <ctype.h>
#include <assert.h>

#include "options.h"
#include "xwin.h"
#include "autoconf.h"
#include "gui.h"
#include "custom.h"
#include "drawing.h"
#include "inputdevice.h"
#include "statusline.h"

#define STATUSLINE_MS 3000

static const char *numbers = { /* ugly  0123456789CHD%+-PNKV */
	"+++++++--++++-+++++++++++++++++-++++++++++++++++++++++++++++++++++++++++++++-++++++-++++----++---+--------------++++++++++-++++++++++++  +++"
	"+xxxxx+--+xx+-+xxxxx++xxxxx++x+-+x++xxxxx++xxxxx++xxxxx++xxxxx++xxxxx++xxxx+-+x++x+-+xxx++-+xx+-+x---+----------+xxxxx++x+-+x++x++x++x+  +x+"
	"+x+++x+--++x+-+++++x++++++x++x+++x++x++++++x++++++++++x++x+++x++x+++x++x++++-+x++x+-+x++x+--+x++x+--+x+----+++--+x---x++xx++x++x+x+++x+  +x+"
	"+x+-+x+---+x+-+xxxxx++xxxxx++xxxxx++xxxxx++xxxxx+--++x+-+xxxxx++xxxxx++x+----+xxxx+-+x++x+----+x+--+xxx+--+xxx+-+xxxxx++x+x+x++xx+   +x++x+ "
	"+x+++x+---+x+-+x++++++++++x++++++x++++++x++x+++x+--+x+--+x+++x++++++x++x++++-+x++x+-+x++x+---+x+x+--+x+----+++--+x++++++x+x+x++x+x++  +xx+  "
	"+xxxxx+---+x+-+xxxxx++xxxxx+----+x++xxxxx++xxxxx+--+x+--+xxxxx++xxxxx++xxxx+-+x++x+-+xxx+---+x++xx--------------+x+----+x++xx++x++x+  +xx+  "
	"+++++++---+++-++++++++++++++----+++++++++++++++++--+++--++++++++++++++++++++-++++++-++++------------------------+++----+++++++++++++  ++++  "
//   x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x      x  
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

  numptr = numbers + num * TD_DEFAULT_NUM_WIDTH + NUMBERS_NUM * TD_DEFAULT_NUM_WIDTH * y;
  for (j = 0; j < TD_DEFAULT_NUM_WIDTH; j++) {
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

  x_start = totalwidth - TD_PADX - VISIBLE_LEDS * TD_DEFAULT_WIDTH;
  for(led = 0; led < LED_MAX; ++led) {
    if(!(ledmask & (1 << led)))
      x_start += TD_DEFAULT_WIDTH;
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
			struct gui_info_drive *gid = &gui_data.drives[pled];
			int track = gid->drive_track;
			on_rgb = 0x00cc00;
			on_rgb2 = 0x006600;
			off_rgb = 0x003300;
			num1 = -1;
			num2 = track / 10;
			num3 = track % 10;
			on = gid->drive_motor;
      if (gid->drive_writing) {
		    on_rgb = 0xcc0000;
				on_rgb2 = 0x880000;
      }
			half = gui_data.drive_side ? 1 : -1;
			if (gid->df[0] == 0) {
				pen_rgb = ledcolor (0x00aaaaaa, rc, gc, bc);
     }
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
			if (pause_emulation) {
				num1 = -1;
				num2 = -1;
				num3 = 16;
				on_rgb = 0xcccccc;
				off_rgb = 0x000000;
				am = 2;
			} else {
			  int fps = (gui_data.fps + 5) / 10;
			  on = gui_data.powerled;
			  on_rgb = 0xcc0000;
			  off_rgb = 0x330000;
			  am = 3;
			  num1 = fps / 100;
			  num2 = (fps - num1 * 100) / 10;
			  num3 = fps % 10;
			  num1 %= 10;
			  num2 %= 10;
			  if (num1 == 0)
				  am = 2;
      }
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
				num2 = gui_data.cpu_halted >= 10 ? (gui_data.cpu_halted / 10) % 10 : 11;
				num3 = gui_data.cpu_halted % 10;
				am = 2;
			} else {
			  num1 = idle / 100;
			  num2 = (idle - num1 * 100) / 10;
			  num3 = idle % 10;
			  num1 %= 10;
			  num2 %= 10;
			  if (!num1 && !num2) {
					num2 = -2;
				}
				am = num1 > 0 ? 3 : 2;
      }
		} else {
			continue;
		}
		if (half > 0) {
      int halfon = y >= TD_TOTAL_HEIGHT / 2;
			c = ledcolor(on ? (halfon ? on_rgb2 : on_rgb) : off_rgb, rc, gc, bc);
		} else if (half < 0) {
      int halfon = y < TD_TOTAL_HEIGHT / 2;
			c = ledcolor(on ? (halfon ? on_rgb2 : on_rgb) : off_rgb, rc, gc, bc);
		} else {
			c = ledcolor(on ? on_rgb : off_rgb, rc, gc, bc);
		}

		border = 0;
		if (y == 0 || y == TD_TOTAL_HEIGHT - 1) {
			c = ledcolor (TD_BORDER, rc, gc, bc);
			border = 1;
		}

    x = x_start;
		if (!border) {
			putpixel (buf, bpp, x - 1, cb);
    }
    for (j = 0; j < TD_DEFAULT_LED_WIDTH; j++) {
	    putpixel (buf, bpp, x + j, c);
    }
		if (!border) {
			putpixel (buf, bpp, x + j, cb);
		}

	  if (y >= TD_PADY && y - TD_PADY < TD_DEFAULT_NUM_HEIGHT) {
			if (num3 >= 0) {
				x += (TD_DEFAULT_LED_WIDTH - am * TD_DEFAULT_NUM_WIDTH) / 2;
        if(num1 > 0) {
        	write_tdnumber (buf, bpp, x, y - TD_PADY, num1, pen_rgb, c2);
        	x += TD_DEFAULT_NUM_WIDTH;
        }
				if (num2 >= 0) {
          write_tdnumber (buf, bpp, x, y - TD_PADY, num2, pen_rgb, c2);
        	x += TD_DEFAULT_NUM_WIDTH;
        } else if (num2 < -1) {
					x += TD_DEFAULT_NUM_WIDTH;
				}
        write_tdnumber (buf, bpp, x, y - TD_PADY, num3, pen_rgb, c2);
				x += TD_DEFAULT_NUM_WIDTH;
      }
	  }
	  x_start += TD_DEFAULT_WIDTH;
  }
}

#define MAX_STATUSLINE_QUEUE 8
struct statusline_struct
{
	TCHAR *text;
	int type;
};
struct statusline_struct statusline_data[MAX_STATUSLINE_QUEUE];
static TCHAR *statusline_text_active;
static int statusline_delay;

static void statusline_update_notification(void)
{
	statusline_updated();
}

void statusline_clear(void)
{
	statusline_text_active = NULL;
	statusline_delay = 0;
	for (int i = 0; i < MAX_STATUSLINE_QUEUE; i++) {
		xfree(statusline_data[i].text);
		statusline_data[i].text = NULL;
	}
	statusline_update_notification();
}

const TCHAR *statusline_fetch(void)
{
	return statusline_text_active;
}

void statusline_add_message(int statustype, const TCHAR *format, ...)
{
	va_list parms;
	TCHAR buffer[256];

	va_start(parms, format);
	buffer[0] = ' ';
	_vsntprintf(buffer + 1, 256 - 2, format, parms);
	_tcscat(buffer, _T(" "));

	for (int i = 0; i < MAX_STATUSLINE_QUEUE; i++) {
		if (statusline_data[i].text != NULL && statusline_data[i].type == statustype) {
			xfree(statusline_data[i].text);
			statusline_data[i].text = NULL;
			for (int j = i + 1; j < MAX_STATUSLINE_QUEUE; j++) {
				memcpy(&statusline_data[j - 1], &statusline_data[j], sizeof(struct statusline_struct));
			}
			statusline_data[MAX_STATUSLINE_QUEUE - 1].text = NULL;
		}
	}

	if (statusline_data[1].text) {
		for (int i = 0; i < MAX_STATUSLINE_QUEUE; i++) {
			if (statusline_data[i].text && !_tcscmp(statusline_data[i].text, buffer)) {
				xfree(statusline_data[i].text);
				for (int j = i + 1; j < MAX_STATUSLINE_QUEUE; j++) {
					memcpy(&statusline_data[j - 1], &statusline_data[j], sizeof(struct statusline_struct));
				}
				statusline_data[MAX_STATUSLINE_QUEUE - 1].text = NULL;
				i = 0;
			}
		}
	} else if (statusline_data[0].text) {
		if (!_tcscmp(statusline_data[0].text, buffer))
			return;
	}

	for (int i = 0; i < MAX_STATUSLINE_QUEUE; i++) {
		if (statusline_data[i].text == NULL) {
			statusline_data[i].text = my_strdup(buffer);
			statusline_data[i].type = statustype;
			if (i == 0)
				statusline_delay = STATUSLINE_MS * vblank_hz / (1000 * 1);
			statusline_text_active = statusline_data[0].text;
			statusline_update_notification();
			return;
		}
	}
	statusline_text_active = NULL;
	xfree(statusline_data[0].text);
	for (int i = 1; i < MAX_STATUSLINE_QUEUE; i++) {
		memcpy(&statusline_data[i - 1], &statusline_data[i], sizeof(struct statusline_struct));
	}
	statusline_data[MAX_STATUSLINE_QUEUE - 1].text = my_strdup(buffer);
	statusline_data[MAX_STATUSLINE_QUEUE - 1].type = statustype;
	statusline_text_active = statusline_data[0].text;
	statusline_update_notification();

	va_end(parms);
}

void statusline_vsync(void)
{
	if (!statusline_data[0].text)
		return;
	if (statusline_delay == 0)
		statusline_delay = STATUSLINE_MS * vblank_hz / (1000 * 1);
	if (statusline_delay > STATUSLINE_MS * vblank_hz / (1000 * 1))
		statusline_delay = STATUSLINE_MS * vblank_hz / (1000 * 1);
	if (statusline_delay > STATUSLINE_MS * vblank_hz / (1000 * 3) && statusline_data[1].text)
		statusline_delay = STATUSLINE_MS * vblank_hz / (1000 * 3);
	statusline_delay--;
	if (statusline_delay)
		return;
	statusline_text_active = NULL;
	xfree(statusline_data[0].text);
	for (int i = 1; i < MAX_STATUSLINE_QUEUE; i++) {
		statusline_data[i - 1].text = statusline_data[i].text;
	}
	statusline_data[MAX_STATUSLINE_QUEUE - 1].text = NULL;
	statusline_text_active = statusline_data[0].text;
	statusline_update_notification();
}
