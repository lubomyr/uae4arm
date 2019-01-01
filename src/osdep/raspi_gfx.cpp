#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "gui.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "savestate.h"
#include "picasso96.h"

#include <png.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>
#include "threaddep/thread.h"
#include "bcm_host.h"

#define DISPLAY_SIGNAL_SETUP 				1
#define DISPLAY_SIGNAL_SUBSHUTDOWN 	2
#define DISPLAY_SIGNAL_OPEN 				3
#define DISPLAY_SIGNAL_SHOW 				4
#define DISPLAY_SIGNAL_QUIT 				5
static uae_thread_id display_tid = 0;
static smp_comm_pipe *volatile display_pipe = 0;
static uae_sem_t display_sem = 0;
static bool volatile display_thread_busy = false;
static int display_width;
static int display_height;


/* SDL surface variable for output of emulation */
SDL_Surface *prSDLScreen = NULL;

unsigned long time_per_frame = 20000; // Default for PAL (50 Hz): 20000 microsecs
static unsigned long last_synctime;
static int host_hz = 50;
static int currVSyncRate = 0;

/* Possible screen modes (x and y resolutions) */
#define MAX_SCREEN_MODES 14
static int x_size_table[MAX_SCREEN_MODES] = {640, 640, 720, 800, 800, 960, 1024, 1280, 1280, 1280, 1360, 1366, 1680, 1920};
static int y_size_table[MAX_SCREEN_MODES] = {400, 480, 400, 480, 600, 540, 768, 720, 800, 1024, 768, 768, 1050, 1080};

struct PicassoResolution *DisplayModes;
struct MultiDisplay Displays[MAX_DISPLAYS];

int screen_is_picasso = 0;

static SDL_Surface *current_screenshot = NULL;
static char screenshot_filename_default[MAX_DPATH] =  {
	'/', 't', 'm', 'p', '/', 'n', 'u', 'l', 'l', '.', 'p', 'n', 'g', '\0'
};
char *screenshot_filename = (char *)&screenshot_filename_default[0];
FILE *screenshot_file = NULL;
static void CreateScreenshot(void);
static int save_thumb(char *path);
int delay_savestate_frame = 0;

DISPMANX_DISPLAY_HANDLE_T   dispmanxdisplay;
DISPMANX_MODEINFO_T         dispmanxdinfo;
DISPMANX_RESOURCE_HANDLE_T  dispmanxresource_amigafb_1 = 0;
DISPMANX_RESOURCE_HANDLE_T  dispmanxresource_amigafb_2 = 0;
DISPMANX_ELEMENT_HANDLE_T   dispmanxelement;
DISPMANX_UPDATE_HANDLE_T    dispmanxupdate;
VC_RECT_T       src_rect;
VC_RECT_T       dst_rect;
VC_RECT_T       blit_rect;

static int DispManXElementpresent = 0;
static unsigned char current_resource_amigafb = 0;

static bool calibrate_done = false;
static const int calibrate_seconds = 5;
static int calibrate_frames = -1;
static unsigned long calibrate_total = 0;
static unsigned long time_for_host_hz_frames = 1000000;
static unsigned long time_per_host_frame = 1000000 / 50;

static volatile uae_atomic host_frame = 0;
static unsigned long host_frame_timestamp = 0;
static int amiga_frame = 0;
static bool sync_was_bogus = true;
static const int sync_epsilon = 1000;
static unsigned long next_amiga_frame_ends = 0;
extern void sound_adjust(float factor);


void vsync_callback(unsigned int a, void* b)
{
  atomic_inc(&host_frame);

  unsigned long currsync = read_processor_time();
  if(host_frame_timestamp == 0) {
    // first sync after start or after bogus frame
    host_frame_timestamp = currsync;
    sync_was_bogus = true;
    return;
  }
  unsigned long diff = currsync - host_frame_timestamp;
  if(diff < time_per_host_frame - sync_epsilon || diff > time_per_host_frame + sync_epsilon) {
    if(diff >= time_per_host_frame * 2 - sync_epsilon && diff < time_per_host_frame * 2 + sync_epsilon) {
      // two frames since last vsync
      atomic_inc(&host_frame);
      host_frame_timestamp = currsync;
      sync_was_bogus = false;
    } else {
      // Bogus frame -> ignore this and next
      host_frame_timestamp = 0;
      sync_was_bogus = true;
    }
    return; // no calibrate if two frames or bogus frame
  }

  sync_was_bogus = false;
  host_frame_timestamp = currsync;

  if(!calibrate_done) {
    if(calibrate_frames < 0) {
      calibrate_frames = calibrate_seconds * host_hz;
    }
    calibrate_frames--;
    calibrate_total += diff;
    if(calibrate_frames == 0) {
      // done
      time_for_host_hz_frames = calibrate_total / calibrate_seconds;
      time_per_host_frame = time_for_host_hz_frames / host_hz;
      time_per_frame = time_for_host_hz_frames / (currprefs.chipset_refreshrate);
      sound_adjust(1000000.0f / (float)time_for_host_hz_frames);
      write_log("calibrate: time_for_host_hz_frames = %ld\n", time_for_host_hz_frames);
      calibrate_frames = -1;
      calibrate_total = 0;
      calibrate_done = true;
    }
  }
}


void wait_for_vsync(void)
{
  unsigned long start = read_processor_time();
  int wait_till = host_frame;
  do {
    usleep(10);
  } while (wait_till >= host_frame && read_processor_time() - start < 500000); // wait max. 0.5 sec
}

void reset_sync(void)
{
  // Wait for sync without bogus frame
  int loop = 0;
  while(loop < 4) { // sync makes no sense if 4 bogus frames in a row...
    wait_for_vsync();
    if(!sync_was_bogus)
      break;
    ++loop;
  }
  
  // reset counter
  host_frame = 0;
  amiga_frame = 0;
  next_amiga_frame_ends = host_frame_timestamp + time_per_frame;
}


static void *display_thread (void *unused)
{
	VC_DISPMANX_ALPHA_T alpha = {
		(DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS), 
		255 /*alpha 0->255*/ , 	0
	};
	uint32_t vc_image_ptr;
	SDL_Surface *Dummy_prSDLScreen;
	int width, height;
	bool callback_registered = false;
	
  for(;;) {
    display_thread_busy = false;
    uae_u32 signal = read_comm_pipe_u32_blocking(display_pipe);
    display_thread_busy = true;
    switch(signal) {
      case DISPLAY_SIGNAL_SETUP:
        if(!callback_registered) {
  				bcm_host_init();
  				dispmanxdisplay = vc_dispmanx_display_open(0);
  			  vc_dispmanx_vsync_callback(dispmanxdisplay, vsync_callback, NULL);
  			  callback_registered = true;
  			}
			  break;

			case DISPLAY_SIGNAL_SUBSHUTDOWN:
				if (DispManXElementpresent == 1) {
					DispManXElementpresent = 0;
					dispmanxupdate = vc_dispmanx_update_start(0);
					vc_dispmanx_element_remove(dispmanxupdate, dispmanxelement);
					vc_dispmanx_update_submit_sync(dispmanxupdate);
				}
			
				if (dispmanxresource_amigafb_1 != 0) {
					vc_dispmanx_resource_delete(dispmanxresource_amigafb_1);
					dispmanxresource_amigafb_1 = 0;
			  }
				if (dispmanxresource_amigafb_2 != 0) {
					vc_dispmanx_resource_delete(dispmanxresource_amigafb_2);
					dispmanxresource_amigafb_2 = 0;
			  }
			  
			  if(prSDLScreen != NULL) {
			    SDL_FreeSurface(prSDLScreen);
			    prSDLScreen = NULL;
			  }
        uae_sem_post (&display_sem);
        break;
				
			case DISPLAY_SIGNAL_OPEN:
				width = display_width;
				height = display_height;
				
				Dummy_prSDLScreen = SDL_SetVideoMode(width, height, 16, SDL_SWSURFACE | SDL_FULLSCREEN);
				prSDLScreen = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height, 16,
					Dummy_prSDLScreen->format->Rmask,	Dummy_prSDLScreen->format->Gmask, Dummy_prSDLScreen->format->Bmask, Dummy_prSDLScreen->format->Amask);
				SDL_FreeSurface(Dummy_prSDLScreen);
			
				vc_dispmanx_display_get_info(dispmanxdisplay, &dispmanxdinfo);
			
				dispmanxresource_amigafb_1 = vc_dispmanx_resource_create(VC_IMAGE_RGB565, width, height, &vc_image_ptr);
				dispmanxresource_amigafb_2 = vc_dispmanx_resource_create(VC_IMAGE_RGB565, width, height, &vc_image_ptr);
			
				vc_dispmanx_rect_set(&blit_rect, 0, 0, width, height);
				vc_dispmanx_resource_write_data(dispmanxresource_amigafb_1, VC_IMAGE_RGB565, prSDLScreen->pitch, prSDLScreen->pixels, &blit_rect);
				vc_dispmanx_rect_set(&src_rect, 0, 0, width << 16, height << 16);
			
				// 16/9 to 4/3 ratio adaptation.
				if (currprefs.gfx_correct_aspect == 0 || screen_is_picasso) {
				  // Fullscreen.
					int scaled_width = dispmanxdinfo.width * currprefs.gfx_fullscreen_ratio / 100;
					int scaled_height = dispmanxdinfo.height * currprefs.gfx_fullscreen_ratio / 100;
					vc_dispmanx_rect_set( &dst_rect, (dispmanxdinfo.width - scaled_width)/2, (dispmanxdinfo.height - scaled_height)/2,
						scaled_width,	scaled_height);
				}	else {
				  // 4/3 shrink.
					int scaled_width = dispmanxdinfo.width * currprefs.gfx_fullscreen_ratio / 100;
					int scaled_height = dispmanxdinfo.height * currprefs.gfx_fullscreen_ratio / 100;
					vc_dispmanx_rect_set( &dst_rect, (dispmanxdinfo.width - scaled_width / 16 * 12)/2, (dispmanxdinfo.height - scaled_height)/2,
						scaled_width/16*12,	scaled_height);
				}
			
				if (DispManXElementpresent == 0) {
					DispManXElementpresent = 1;
					dispmanxupdate = vc_dispmanx_update_start(0);
					dispmanxelement = vc_dispmanx_element_add(dispmanxupdate, dispmanxdisplay,	2,               // layer
						&dst_rect, dispmanxresource_amigafb_1, &src_rect,	DISPMANX_PROTECTION_NONE,	&alpha,	NULL, DISPMANX_NO_ROTATE);
			
					vc_dispmanx_update_submit(dispmanxupdate, NULL, NULL);
				}
        uae_sem_post (&display_sem);
        break;

			case DISPLAY_SIGNAL_SHOW:
				if (current_resource_amigafb == 1) {
					current_resource_amigafb = 0;
				  vc_dispmanx_resource_write_data(dispmanxresource_amigafb_1, VC_IMAGE_RGB565,
					  adisplays.gfxvidinfo.drawbuffer.rowbytes, adisplays.gfxvidinfo.drawbuffer.bufmem, &blit_rect);
				  dispmanxupdate = vc_dispmanx_update_start(0);
				  vc_dispmanx_element_change_source(dispmanxupdate, dispmanxelement, dispmanxresource_amigafb_1);
				} else {
					current_resource_amigafb = 1;
					vc_dispmanx_resource_write_data(dispmanxresource_amigafb_2,	VC_IMAGE_RGB565,
						adisplays.gfxvidinfo.drawbuffer.rowbytes,	adisplays.gfxvidinfo.drawbuffer.bufmem,	&blit_rect);
					dispmanxupdate = vc_dispmanx_update_start(0);
					vc_dispmanx_element_change_source(dispmanxupdate, dispmanxelement, dispmanxresource_amigafb_2);
				}
			  vc_dispmanx_update_submit(dispmanxupdate, NULL, NULL);
				break;
								
      case DISPLAY_SIGNAL_QUIT:
        callback_registered = false;
			  vc_dispmanx_vsync_callback(dispmanxdisplay, NULL, NULL);
				vc_dispmanx_display_close(dispmanxdisplay);
				bcm_host_deinit();
				SDL_VideoQuit();
        display_tid = 0;
        return 0;
    }
  }
}


int graphics_setup(void)
{
#ifdef PICASSO96
	picasso_InitResolutions();
	InitPicasso96();
#endif
  VCHI_INSTANCE_T vchi_instance;
  VCHI_CONNECTION_T *vchi_connection;
  TV_DISPLAY_STATE_T tvstate;

  if(vchi_initialise(&vchi_instance) == 0) {
    if(vchi_connect(NULL, 0, vchi_instance) == 0) {
      vc_vchi_tv_init(vchi_instance, &vchi_connection, 1);
      if(vc_tv_get_display_state(&tvstate) == 0) {
        HDMI_PROPERTY_PARAM_T property;
        property.property = HDMI_PROPERTY_PIXEL_CLOCK_TYPE;
        vc_tv_hdmi_get_property(&property);
        float frame_rate = property.param1 == HDMI_PIXEL_CLOCK_TYPE_NTSC ? tvstate.display.hdmi.frame_rate * (1000.0f/1001.0f) : tvstate.display.hdmi.frame_rate;
        host_hz = (int)frame_rate;
        time_per_host_frame = time_for_host_hz_frames / host_hz;
      }
      vc_vchi_tv_stop();
      vchi_disconnect(vchi_instance);
    }
  }

  if(display_pipe == 0) {
    display_pipe = xmalloc (smp_comm_pipe, 1);
    init_comm_pipe(display_pipe, 20, 1);
  }
  if(display_sem == 0) {
    uae_sem_init (&display_sem, 0, 0);
  }
  if(display_tid == 0 && display_pipe != 0 && display_sem != 0) {
    uae_start_thread(_T("render"), display_thread, NULL, &display_tid);
  }
	write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SETUP, 1);
	
	return 1;
}


static void wait_for_display_thread(void)
{
	while(display_thread_busy)
		usleep(10);
}


#ifdef WITH_LOGGING

SDL_Surface *liveInfo = NULL;
TTF_Font *liveFont = NULL;
int liveInfoCounter = 0;
void ShowLiveInfo(char *msg)
{
  if(liveFont == NULL) {
    TTF_Init();
    liveFont = TTF_OpenFont("data/FreeSans.ttf", 12);
  }
  if(liveInfo != NULL) {
    SDL_FreeSurface(liveInfo);
    liveInfo = NULL;
  }
  SDL_Color col;
  col.r = 0xbf;
  col.g = 0xbf;
  col.b = 0xbf;
  liveInfo = TTF_RenderText_Solid(liveFont, msg, col);
  liveInfoCounter = 50 * 5;
}

void RefreshLiveInfo()
{
  if(liveInfoCounter > 0)
  {
    SDL_Rect dst, src;
    
    dst.x = 0;
    dst.y = 2;
    src.w = liveInfo->w;
    src.h = liveInfo->h;
    src.x = 0;
    src.y = 0;
    if(liveInfo != NULL)
      SDL_BlitSurface(liveInfo, &src, prSDLScreen, &dst);
    liveInfoCounter--;
    if(liveInfoCounter == 0) {
      if(liveInfo != NULL) {
        SDL_FreeSurface(liveInfo);
        liveInfo = NULL;
      }
    }
  }
}

#endif

void InitAmigaVidMode(struct uae_prefs *p)
{
  /* Initialize structure for Amiga video modes */
	struct amigadisplay *ad = &adisplays;
	ad->gfxvidinfo.drawbuffer.pixbytes = 2;
	ad->gfxvidinfo.drawbuffer.bufmem = (uae_u8 *)prSDLScreen->pixels;
	ad->gfxvidinfo.drawbuffer.outwidth = p->gfx_monitor.gfx_size.width;
	ad->gfxvidinfo.drawbuffer.outheight = p->gfx_monitor.gfx_size.height << p->gfx_vresolution;
	ad->gfxvidinfo.drawbuffer.rowbytes = prSDLScreen->pitch;
}


void graphics_subshutdown(void)
{
	if(display_tid != 0) {
		wait_for_display_thread();
		write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SUBSHUTDOWN, 1);
	  uae_sem_wait (&display_sem);
	}
}

void graphics_thread_leave(void)
{
	if(display_tid != 0) {
	  write_comm_pipe_u32 (display_pipe, DISPLAY_SIGNAL_QUIT, 1);
	  while(display_tid != 0) {
	    sleep_millis(10);
	  }
	  destroy_comm_pipe(display_pipe);
	  xfree(display_pipe);
	  display_pipe = 0;
	  uae_sem_destroy(&display_sem);
	  display_sem = 0;
	}
}


static void open_screen(struct uae_prefs *p)
{
  graphics_subshutdown();
  
  current_resource_amigafb = 0;

	currprefs.gfx_correct_aspect = changed_prefs.gfx_correct_aspect;
	currprefs.gfx_fullscreen_ratio = changed_prefs.gfx_fullscreen_ratio;

#ifdef PICASSO96
	if (screen_is_picasso) {
		display_width = picasso_vidinfo.width ? picasso_vidinfo.width : 640;
		display_height = picasso_vidinfo.height ? picasso_vidinfo.height : 256;
	} else
#endif
	{
		p->gfx_resolution = p->gfx_monitor.gfx_size.width ? (p->gfx_monitor.gfx_size.width > 600 ? 1 : 0) : 1;
		display_width = p->gfx_monitor.gfx_size.width ? p->gfx_monitor.gfx_size.width : 640;
		display_height = (p->gfx_monitor.gfx_size.height ? p->gfx_monitor.gfx_size.height : 256) << p->gfx_vresolution;
	}

	write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_OPEN, 1);
  uae_sem_wait (&display_sem);
  
	if (prSDLScreen != NULL) {
		InitAmigaVidMode(p);
		init_row_map();
	}    

  calibrate_done = false;
  reset_sync();
}


void update_display(struct uae_prefs *p)
{
	struct amigadisplay *ad = &adisplays;

	open_screen(p);
  
	SDL_ShowCursor(SDL_DISABLE);

	ad->framecnt = 1; // Don't draw frame before reset done
}


int check_prefs_changed_gfx(void)
{
	int changed = 0;
  
	if (currprefs.gfx_monitor.gfx_size.height != changed_prefs.gfx_monitor.gfx_size.height ||
	   currprefs.gfx_monitor.gfx_size.width != changed_prefs.gfx_monitor.gfx_size.width ||
	   currprefs.gfx_resolution != changed_prefs.gfx_resolution ||
		 currprefs.gfx_vresolution != changed_prefs.gfx_vresolution)
	{
		currprefs.gfx_monitor.gfx_size.height = changed_prefs.gfx_monitor.gfx_size.height;
		currprefs.gfx_monitor.gfx_size.width = changed_prefs.gfx_monitor.gfx_size.width;
		currprefs.gfx_resolution = changed_prefs.gfx_resolution;
		currprefs.gfx_vresolution = changed_prefs.gfx_vresolution;
		update_display(&currprefs);
		changed = 1;
	}
	if (currprefs.leds_on_screen != changed_prefs.leds_on_screen ||
	    currprefs.leds_on_screen_mask[0] != changed_prefs.leds_on_screen_mask[0] ||
	    currprefs.leds_on_screen_mask[1] != changed_prefs.leds_on_screen_mask[1] ||
	    currprefs.gfx_monitor.gfx_size.y != changed_prefs.gfx_monitor.gfx_size.y)	
	{
		currprefs.leds_on_screen = changed_prefs.leds_on_screen;
		currprefs.leds_on_screen_mask[0] = changed_prefs.leds_on_screen_mask[0];
		currprefs.leds_on_screen_mask[1] = changed_prefs.leds_on_screen_mask[1];
		currprefs.gfx_monitor.gfx_size.y = changed_prefs.gfx_monitor.gfx_size.y;
		changed = 1;
	}
	if (currprefs.chipset_refreshrate != changed_prefs.chipset_refreshrate) {
		currprefs.chipset_refreshrate = changed_prefs.chipset_refreshrate;
		init_hz_normal();
		changed = 1;
	}
  
	// Not the correct place for this...
	currprefs.filesys_limit = changed_prefs.filesys_limit;
	currprefs.harddrive_read_only = changed_prefs.harddrive_read_only;
  
  if(changed) {
    inputdevice_unacquire();
		init_custom ();
		inputdevice_acquire(TRUE);
  }

	return changed;
}


int lockscr(void)
{
  if(SDL_LockSurface(prSDLScreen) == -1)
    return 0;
  init_row_map();
	return 1;
}


void unlockscr(void)
{
  SDL_UnlockSurface(prSDLScreen);
}

bool render_screen (bool immediate)
{
	if (savestate_state == STATE_DOSAVE) {
		if (delay_savestate_frame > 0)
			--delay_savestate_frame;
		else {
			CreateScreenshot();
			save_thumb(screenshot_filename);
			savestate_state = 0;
		}
	}

#ifdef WITH_LOGGING
  RefreshLiveInfo();
#endif
  
	return true;
}

void show_screen(int mode)
{
  unsigned long start = read_processor_time();

  last_synctime = start;
  while(last_synctime < next_amiga_frame_ends && last_synctime < start + time_per_frame) {
    usleep(10);
    last_synctime = read_processor_time();
  }
  amiga_frame = amiga_frame + 1 + currprefs.gfx_framerate;

  if(amiga_frame == currVSyncRate) {
    bool do_check = true;
    if(host_frame < host_hz) {
      // we are here before relevant vsync occured
      wait_for_vsync();
      if(sync_was_bogus)
        do_check = false;
    }
    if(do_check && host_frame == host_hz) {
      // Check time difference between Amiga and host sync.
      // If difference is too big, slightly ajust time per frame.
      long diff;
      static long last_diff = 0;
      if(next_amiga_frame_ends > host_frame_timestamp) {
        diff = next_amiga_frame_ends - host_frame_timestamp;
        if(diff > 50 && last_diff != 0 && diff > last_diff)
          time_per_frame--;
        last_diff = diff;
      } else {
        diff = host_frame_timestamp - next_amiga_frame_ends;
        if(diff > time_per_frame / 2) {
          next_amiga_frame_ends += time_per_frame;
          diff = -diff;
          last_diff = 0;
        } else {
          diff = -diff;
          if(diff < -50 && last_diff != 0 && diff < last_diff)
            time_per_frame++;
          last_diff = diff;
        }
      }
      write_log("Diff Amiga frame to host: %6d, time_per_frame = %6d\n", diff, time_per_frame);
    }
    host_frame = 0;
    amiga_frame = 0;
  }
  
  next_amiga_frame_ends += time_per_frame;
  if(currprefs.gfx_framerate)
    next_amiga_frame_ends += time_per_frame;
    
	wait_for_display_thread();
	write_comm_pipe_u32(display_pipe, DISPLAY_SIGNAL_SHOW, 1);

  idletime += last_synctime - start;
}


unsigned long target_lastsynctime(void)
{
  return last_synctime;
}

void black_screen_now(void)
{
  if(prSDLScreen != NULL) {	
    SDL_FillRect(prSDLScreen, NULL, 0);
    render_screen(true);
	  show_screen(0);
  }
}


static void graphics_subinit(void)
{
	if (prSDLScreen == NULL) {
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return;
	} else {
		SDL_ShowCursor(SDL_DISABLE);
		InitAmigaVidMode(&currprefs);
	}
}


static int init_colors(void)
{
	int red_bits, green_bits, blue_bits;
	int red_shift, green_shift, blue_shift;

	/* Truecolor: */
	red_bits = bits_in_mask(prSDLScreen->format->Rmask);
	green_bits = bits_in_mask(prSDLScreen->format->Gmask);
	blue_bits = bits_in_mask(prSDLScreen->format->Bmask);
	red_shift = mask_shift(prSDLScreen->format->Rmask);
	green_shift = mask_shift(prSDLScreen->format->Gmask);
	blue_shift = mask_shift(prSDLScreen->format->Bmask);
	alloc_colors64k(red_bits, green_bits, blue_bits, red_shift, green_shift, blue_shift);
	notice_new_xcolors();

	return 1;
}


/*
 * Find the colour depth of the display
 */
static int get_display_depth(void)
{
	const SDL_VideoInfo *vid_info;
	int depth = 0;

	if ((vid_info = SDL_GetVideoInfo())) {
		depth = vid_info->vfmt->BitsPerPixel;

		/* Don't trust the answer if it's 16 bits; the display
		* could actually be 15 bits deep. We'll count the bits
		* ourselves */
		if (depth == 16)
			depth = bits_in_mask(vid_info->vfmt->Rmask) + bits_in_mask(vid_info->vfmt->Gmask) + bits_in_mask(vid_info->vfmt->Bmask);
	}
	return depth;
}

int GetSurfacePixelFormat(void)
{
	int depth = get_display_depth();
	int unit = (depth + 1) & 0xF8;

	return (unit == 8 ? RGBFB_CHUNKY
		  : depth == 15 && unit == 16 ? RGBFB_R5G5B5
		  : depth == 16 && unit == 16 ? RGBFB_R5G6B5
		  : unit == 24 ? RGBFB_B8G8R8
		  : unit == 32 ? RGBFB_R8G8B8A8
		  : RGBFB_NONE);
}


int graphics_init(bool mousecapture)
{
	inputdevice_unacquire();

	graphics_subinit();

	if (!init_colors())
		return 0;
  
	inputdevice_acquire(TRUE);

	return 1;
}

void graphics_leave(void)
{
	graphics_subshutdown();
}


#define  systemRedShift      (prSDLScreen->format->Rshift)
#define  systemGreenShift    (prSDLScreen->format->Gshift)
#define  systemBlueShift     (prSDLScreen->format->Bshift)
#define  systemRedMask       (prSDLScreen->format->Rmask)
#define  systemGreenMask     (prSDLScreen->format->Gmask)
#define  systemBlueMask      (prSDLScreen->format->Bmask)

static int save_png(SDL_Surface* surface, char *path)
{
	int w = surface->w;
	int h = surface->h;
	unsigned char * pix = (unsigned char *)surface->pixels;
	unsigned char writeBuffer[1024 * 3];
	FILE *f  = fopen(path, "wb");
	if (!f) return 0;
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(f);
		return 0;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);

	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(f);
		return 0;
	}

	png_init_io(png_ptr, f);
	png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	unsigned char *b = writeBuffer;

	int sizeX = w;
	int sizeY = h;
	int y;
	int x;

	unsigned short *p = (unsigned short *)pix;
	for (y = 0; y < sizeY; y++) {
		for (x = 0; x < sizeX; x++) {
			unsigned short v = p[x];
  
			*b++ = ((v & systemRedMask) >> systemRedShift) << 3; // R
			*b++ = ((v & systemGreenMask) >> systemGreenShift) << 2; // G 
			*b++ = ((v & systemBlueMask) >> systemBlueShift) << 3; // B
		}
		p += surface->pitch / 2;
		png_write_row(png_ptr, writeBuffer);
		b = writeBuffer;
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(f);
	return 1;
}


static void CreateScreenshot(void)
{
	int w, h;

	if (current_screenshot != NULL) {
		SDL_FreeSurface(current_screenshot);
		current_screenshot = NULL;
	}

  if(prSDLScreen != NULL) {
  	w = prSDLScreen->w;
  	h = prSDLScreen->h;
  	current_screenshot = SDL_CreateRGBSurfaceFrom(prSDLScreen->pixels, w, h, prSDLScreen->format->BitsPerPixel,	prSDLScreen->pitch,
  		prSDLScreen->format->Rmask, prSDLScreen->format->Gmask, prSDLScreen->format->Bmask, prSDLScreen->format->Amask);
	}
}


static int save_thumb(char *path)
{
	int ret = 0;
	if (current_screenshot != NULL)	{
		ret = save_png(current_screenshot, path);
		SDL_FreeSurface(current_screenshot);
		current_screenshot = NULL;
	}
	return ret;
}


bool vsync_switchmode(int hz)
{
	int changed_height = changed_prefs.gfx_monitor.gfx_size.height;
	struct amigadisplay *ad = &adisplays;
	
	if (hz >= 55)
		hz = 60;
	else
		hz = 50;

	if (hz == 50 && currVSyncRate == 60) {
	  // Switch from NTSC -> PAL
		switch (changed_height) {
		case 200: changed_height = 240; break;
		case 216: changed_height = 262; break;
		case 240: changed_height = 270; break;
		case 256: changed_height = 270; break;
		case 262: changed_height = 270; break;
		case 270: changed_height = 270; break;
		}
	}	else if (hz == 60 && currVSyncRate == 50)	{
	  // Switch from PAL -> NTSC
		switch (changed_height) {
		case 200: changed_height = 200; break;
		case 216: changed_height = 200; break;
		case 240: changed_height = 200; break;
		case 256: changed_height = 216; break;
		case 262: changed_height = 216; break;
		case 270: changed_height = 240; break;
		}
	}

  if(hz != currVSyncRate) {
    currVSyncRate = hz;
  	black_screen_now();
    fpscounter_reset();
    time_per_frame = time_for_host_hz_frames / (hz);
  }
  
  if(!ad->picasso_on && !ad->picasso_requested_on)
  	changed_prefs.gfx_monitor.gfx_size.height = changed_height;

	return true;
}

bool target_graphics_buffer_update(void)
{
	bool rate_changed = false;
  
	if (currprefs.gfx_monitor.gfx_size.height != changed_prefs.gfx_monitor.gfx_size.height) {
		update_display(&changed_prefs);
		rate_changed = true;
	}

	if (rate_changed) {
		black_screen_now();
		fpscounter_reset();
		time_per_frame = time_for_host_hz_frames / (currprefs.chipset_refreshrate);
	}

  reset_sync();

	return true;
}

#ifdef PICASSO96


int picasso_palette(struct MyCLUTEntry *CLUT)
{
	int i, changed;

	changed = 0;
	for (i = 0; i < 256; i++) {
    int r = CLUT[i].Red;
    int g = CLUT[i].Green;
    int b = CLUT[i].Blue;
		int value = (r << 16 | g << 8 | b);
		uae_u32 v = CONVERT_RGB(value);
		if (v !=  picasso_vidinfo.clut[i]) {
			picasso_vidinfo.clut[i] = v;
			changed = 1;
		} 
	}
	return changed;
}

static int resolution_compare(const void *a, const void *b)
{
	struct PicassoResolution *ma = (struct PicassoResolution *)a;
	struct PicassoResolution *mb = (struct PicassoResolution *)b;
	if (ma->res.width < mb->res.width)
		return -1;
	if (ma->res.width > mb->res.width)
		return 1;
	if (ma->res.height < mb->res.height)
		return -1;
	if (ma->res.height > mb->res.height)
		return 1;
	return ma->depth - mb->depth;
}
static void sortmodes(void)
{
	int	i = 0, idx = -1;
	int pw = -1, ph = -1;
	while (DisplayModes[i].depth >= 0)
		i++;
	qsort(DisplayModes, i, sizeof(struct PicassoResolution), resolution_compare);
	for (i = 0; DisplayModes[i].depth >= 0; i++) {
		if (DisplayModes[i].res.height != ph || DisplayModes[i].res.width != pw) {
			ph = DisplayModes[i].res.height;
			pw = DisplayModes[i].res.width;
			idx++;
		}
		DisplayModes[i].residx = idx;
	}
}

static void modesList(void)
{
	int i, j;

	i = 0;
	while (DisplayModes[i].depth >= 0) {
		write_log("%d: %s (", i, DisplayModes[i].name);
		j = 0;
		while (DisplayModes[i].refresh[j] > 0) {
			if (j > 0)
				write_log(",");
			write_log("%d", DisplayModes[i].refresh[j]);
			j++;
		}
		write_log(")\n");
		i++;
	}
}

void picasso_InitResolutions(void)
{
	struct MultiDisplay *md1;
	int i, count = 0;
	char tmp[200];
	int bit_idx;
	int bits[] = { 8, 16, 32 };
  
	Displays[0].primary = 1;
	Displays[0].disabled = 0;
	Displays[0].rect.left = 0;
	Displays[0].rect.top = 0;
	Displays[0].rect.right = 800;
	Displays[0].rect.bottom = 640;
	sprintf(tmp, "%s (%d*%d)", "Display", Displays[0].rect.right, Displays[0].rect.bottom);
	Displays[0].name = my_strdup(tmp);
	Displays[0].name2 = my_strdup("Display");

	md1 = Displays;
	DisplayModes = md1->DisplayModes = xmalloc(struct PicassoResolution, MAX_PICASSO_MODES);
	for (i = 0; i < MAX_SCREEN_MODES && count < MAX_PICASSO_MODES; i++) {
		for (bit_idx = 0; bit_idx < 3; ++bit_idx) {
			int bitdepth = bits[bit_idx];
			int bit_unit = (bitdepth + 1) & 0xF8;
			int rgbFormat = (bitdepth == 8 ? RGBFB_CLUT : (bitdepth == 16 ? RGBFB_R5G6B5 : RGBFB_R8G8B8A8));
			int pixelFormat = 1 << rgbFormat;
			pixelFormat |= RGBFF_CHUNKY;
      
			if (SDL_VideoModeOK (x_size_table[i], y_size_table[i], 16, SDL_HWSURFACE)) {
				DisplayModes[count].res.width = x_size_table[i];
				DisplayModes[count].res.height = y_size_table[i];
				DisplayModes[count].depth = bit_unit >> 3;
				DisplayModes[count].refresh[0] = 50;
				DisplayModes[count].refresh[1] = 60;
				DisplayModes[count].refresh[2] = 0;
				DisplayModes[count].colormodes = pixelFormat;
				sprintf(DisplayModes[count].name,	"%dx%d, %d-bit", 
          DisplayModes[count].res.width, DisplayModes[count].res.height, DisplayModes[count].depth * 8);
  
				count++;
			}
		}
	}
	DisplayModes[count].depth = -1;
	sortmodes();
	modesList();
	DisplayModes = Displays[0].DisplayModes;
}


void gfx_set_picasso_state(int on)
{
	if (on == screen_is_picasso)
		return;

	screen_is_picasso = on;
	open_screen(&currprefs);
  if(prSDLScreen != NULL)
  	picasso_vidinfo.rowbytes = prSDLScreen->pitch;
}

void gfx_set_picasso_modeinfo(uae_u32 w, uae_u32 h, uae_u32 depth, RGBFTYPE rgbfmt)
{
	depth >>= 3;
	if (((unsigned)picasso_vidinfo.width == w) &&
	  ((unsigned)picasso_vidinfo.height == h) &&
	  ((unsigned)picasso_vidinfo.depth == depth) &&
	  (picasso_vidinfo.selected_rgbformat == rgbfmt))
		return;

	picasso_vidinfo.selected_rgbformat = rgbfmt;
	picasso_vidinfo.width = w;
	picasso_vidinfo.height = h;
	picasso_vidinfo.depth = 2; // Native depth
	picasso_vidinfo.extra_mem = 1;

	picasso_vidinfo.pixbytes = 2; // Native bytes
	if (screen_is_picasso) {
		open_screen(&currprefs);
  	if(prSDLScreen != NULL)
  		picasso_vidinfo.rowbytes	= prSDLScreen->pitch;
		picasso_vidinfo.rgbformat = RGBFB_R5G6B5;
	}
}

uae_u8 *gfx_lock_picasso(void)
{
  if(prSDLScreen == NULL || screen_is_picasso == 0)
    return NULL;
  SDL_LockSurface(prSDLScreen);
	picasso_vidinfo.rowbytes = prSDLScreen->pitch;
	return (uae_u8 *)prSDLScreen->pixels;
}

void gfx_unlock_picasso(bool dorender)
{
  SDL_UnlockSurface(prSDLScreen);
  if(dorender) {
    render_screen(true);
    show_screen(0);
  }
}

#endif // PICASSO96
