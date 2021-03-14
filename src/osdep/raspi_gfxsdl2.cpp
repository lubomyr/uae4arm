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
#include "statusline.h"

#include <png.h>
#include <SDL.h>
#include "threaddep/thread.h"
#include "bcm_host.h"

static int display_width;
static int display_height;


/* SDL surface variable for output of emulation */
SDL_Surface *prSDLScreen = NULL;

static unsigned int current_vsync_frame = 0;
uae_u32 time_per_frame = 20000; // Default for PAL (50 Hz): 20000 microsecs
static uae_u32 last_synctime;
static int vsync_modulo = 1;
static int host_hz = 50;

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

static uae_u32 next_synctime = 0;

DISPMANX_DISPLAY_HANDLE_T   dispmanxdisplay;

SDL_Window* sdlWindow;
SDL_Renderer* renderer;
SDL_Texture* texture;
SDL_DisplayMode sdlMode;

static volatile uae_atomic vsync_counter = 0;
void vsync_callback(unsigned int a, void* b)
{
  atomic_inc(&vsync_counter);
}

// In case of error, print the error code and close the application
void check_error_sdl(bool check, const char* message) 
{
	if (check) {
		printf("%s %d\n", message, SDL_GetError());
		SDL_Quit();
		exit(-1);
	}
}

static void Create_SDL_Surface(int width, int height, int depth)
{
	prSDLScreen = SDL_CreateRGBSurface(0, width, height, depth, 0, 0, 0, 0);
	check_error_sdl(prSDLScreen == NULL, "Unable to create a surface");
}

static void Create_SDL_Texture(int width, int height, int depth)
{
	if (depth == 16) {
		// Initialize SDL Texture for the renderer
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,	SDL_TEXTUREACCESS_STREAMING,
			width, height);
	}
	else if (depth == 32)
	{
		texture = SDL_CreateTexture(renderer,	SDL_PIXELFORMAT_ARGB32,	SDL_TEXTUREACCESS_STREAMING,
			width, height);
	}
	check_error_sdl(texture == NULL, "Unable to create texture");
}

// Check if the requested Amiga resolution can be displayed with the current Screen mode as a direct multiple
// Based on this we make the decision to use Linear (smooth) or Nearest Neighbor (pixelated) scaling
static bool isModeAspectRatioExact(SDL_DisplayMode* mode, int width, int height)
{
	if (mode->w % width == 0 && mode->h % height == 0)
		return true;
	return false;
}

void updatedisplayarea()
{
	// Update the texture from the surface
	SDL_UpdateTexture(texture, nullptr, prSDLScreen->pixels, prSDLScreen->pitch);
	SDL_RenderClear(renderer);
	// Copy the texture on the renderer
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);
	// Update the window surface (show the renderer)
	SDL_RenderPresent(renderer);
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
      }
      vc_vchi_tv_stop();
      vchi_disconnect(vchi_instance);
    }
  }

	bcm_host_init();
	dispmanxdisplay = vc_dispmanx_display_open(0);
  vc_dispmanx_vsync_callback(dispmanxdisplay, vsync_callback, NULL);

	sdlWindow = SDL_CreateWindow("UAE4ARM-SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
  check_error_sdl(sdlWindow == NULL, "Unable to create window");
		
	renderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
	check_error_sdl(renderer == NULL, "Unable to create a renderer");

	if (SDL_GetWindowDisplayMode(sdlWindow, &sdlMode) != 0)
		SDL_Log("Could not get information about SDL Mode! SDL_Error: %s\n", SDL_GetError());
	
	if (SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1") != SDL_TRUE)
		SDL_Log("SDL could not grab the keyboard");
	
	SDL_ShowCursor(SDL_DISABLE);
	
	return 1;
}


SDL_Surface *liveInfo = NULL;
TTF_Font *liveFont = NULL;
int liveInfoCounter = 0;
void 	statusline_updated(void)
{
  if(liveFont == NULL) {
    TTF_Init();
    liveFont = TTF_OpenFont("data/Hack-Regular.ttf", 10);
  }
  if(liveInfo != NULL) {
    SDL_FreeSurface(liveInfo);
    liveInfo = NULL;
  }
  const TCHAR *text = statusline_fetch();
  if(!text || text[0] == 0) {
    liveInfoCounter = 0;
    return;
  }
  SDL_Color col;
  col.r = 0xdf;
  col.g = 0xdf;
  col.b = 0xdf;
  liveInfo = TTF_RenderText_Solid(liveFont, text, col);
  liveInfoCounter = 50 * 5;
}

void RefreshLiveInfo()
{
  if(liveInfoCounter > 0 && liveInfo != NULL)
  {
    SDL_Rect dst, src;
    
    dst.x = 0;
    dst.y = prSDLScreen->h - liveInfo->h;
    src.w = liveInfo->w;
    src.h = liveInfo->h;
    src.x = 0;
    src.y = 0;
    SDL_BlitSurface(liveInfo, &src, prSDLScreen, &dst);
    liveInfoCounter--;
    if(liveInfoCounter == 0) {
      SDL_FreeSurface(liveInfo);
      liveInfo = NULL;
    }
  }
}


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
  if(prSDLScreen != NULL) {
    SDL_FreeSurface(prSDLScreen);
    prSDLScreen = NULL;
  }
	if (texture != NULL) {
		SDL_DestroyTexture(texture);
    texture = NULL;
	}
}


static void open_screen(struct uae_prefs *p)
{
  graphics_subshutdown();
  
	currprefs.gfx_correct_aspect = changed_prefs.gfx_correct_aspect;
	currprefs.gfx_fullscreen_ratio = changed_prefs.gfx_fullscreen_ratio;

#ifdef PICASSO96
	if (screen_is_picasso)
	{
		display_width = picasso_vidinfo.width ? picasso_vidinfo.width : 640;
		display_height = picasso_vidinfo.height ? picasso_vidinfo.height : 256;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); // we always use linear for Picasso96 modes
	}
	else
#endif
	{
		p->gfx_resolution = p->gfx_monitor.gfx_size.width ? (p->gfx_monitor.gfx_size.width > 600 ? 1 : 0) : 1;
		display_width = p->gfx_monitor.gfx_size.width ? p->gfx_monitor.gfx_size.width : 640;
		display_height = (p->gfx_monitor.gfx_size.height ? p->gfx_monitor.gfx_size.height : 256) << p->gfx_vresolution;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	}

	Create_SDL_Surface(display_width, display_height, 16);

	if (screen_is_picasso)
		SDL_RenderSetLogicalSize(renderer, display_width, display_height >> p->gfx_vresolution);
	else
		SDL_RenderSetLogicalSize(renderer, display_width, (display_height*2) >> p->gfx_vresolution);

	Create_SDL_Texture(display_width, display_height, 16);

	// Update the texture from the surface
	SDL_UpdateTexture(texture, NULL, prSDLScreen->pixels, prSDLScreen->pitch);
	SDL_RenderClear(renderer);
	// Copy the texture on the renderer
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	// Update the window surface (show the renderer)
	SDL_RenderPresent(renderer);
  
  vsync_counter = 0;
  current_vsync_frame = 2;

	if (prSDLScreen != NULL)
	{
		InitAmigaVidMode(p);
		init_row_map();
	}    
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
	if (currprefs.chipset_refreshrate != changed_prefs.chipset_refreshrate) 
	{
		currprefs.chipset_refreshrate = changed_prefs.chipset_refreshrate;
		init_hz_normal();
		changed = 1;
	}
  
	// Not the correct place for this...
	currprefs.filesys_limit = changed_prefs.filesys_limit;
	currprefs.harddrive_read_only = changed_prefs.harddrive_read_only;
  
  if(changed)
		init_custom ();

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

void wait_for_vsync(void)
{
  uae_u32 start = read_processor_time();
  int wait_till = current_vsync_frame;
  do 
  {
    usleep(10);
    current_vsync_frame = vsync_counter;
  } while (wait_till >= current_vsync_frame && read_processor_time() - start < 20000);
}


bool render_screen (void)
{
	if (savestate_state == STATE_DOSAVE)
	{
		if (delay_savestate_frame > 0)
			--delay_savestate_frame;
		else
		{
			CreateScreenshot();
			save_thumb(screenshot_filename);
			savestate_state = 0;
		}
	}

  RefreshLiveInfo();
  
	// Update the texture from the surface
	SDL_UpdateTexture(texture, NULL, prSDLScreen->pixels, prSDLScreen->pitch);
	SDL_RenderClear(renderer);
	// Copy the texture on the renderer
	SDL_RenderCopy(renderer, texture, NULL, NULL);

	return true;
}

void show_screen(int mode)
{
  uae_u32 start = read_processor_time();

  int wait_till = current_vsync_frame;
  if(vsync_modulo == 1) {
    // Amiga framerate is equal to host framerate
    do 
    {
      usleep(10);
      current_vsync_frame = vsync_counter;
    } while (wait_till >= current_vsync_frame && read_processor_time() - start < 40000);

    if(wait_till + 1 != current_vsync_frame) {
      // We missed a vsync...
      next_synctime = 0;
    }
  } else {
    // Amiga framerate differs from host framerate
    uae_u32 wait_till_time = (next_synctime != 0) ? next_synctime : last_synctime + time_per_frame;
    if(current_vsync_frame % vsync_modulo == 0) {
      // Real vsync
      if(start < wait_till_time) {
        // We are in time, wait for vsync
        atomic_set(&vsync_counter, current_vsync_frame);
        do 
        {
          usleep(10);
          current_vsync_frame = vsync_counter;
        } while (wait_till >= current_vsync_frame && read_processor_time() - start < 40000);
      } else {
        // Too late for vsync
      }
    } else {
      // Estimate vsync by time
      while (wait_till_time > read_processor_time()) {
        usleep(10);
      }
      ++current_vsync_frame;
    }
  }

  current_vsync_frame += currprefs.gfx_framerate;

  last_synctime = read_processor_time();

	// Update the window surface (show the renderer)
	SDL_RenderPresent(renderer);

  idletime += last_synctime - start;

  if (last_synctime - next_synctime > time_per_frame - 5000)
    next_synctime = last_synctime + time_per_frame * (1 + currprefs.gfx_framerate);
  else
    next_synctime = next_synctime + time_per_frame * (1 + currprefs.gfx_framerate);
}


uae_u32 target_lastsynctime(void)
{
  return last_synctime;
}

void black_screen_now(void)
{
  if(prSDLScreen != NULL) {	
    SDL_FillRect(prSDLScreen, NULL, 0);
    render_screen();
	  show_screen(0);
  }
}


static void graphics_subinit(void)
{
	if (prSDLScreen == NULL)
	{
		fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
		return;
	}
	else
	{
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
	int depth = prSDLScreen->format->BytesPerPixel == 4 ? 32 : 16;
	return depth;
}

int GetSurfacePixelFormat(void)
{
	int depth = get_display_depth();
	int unit = (depth + 1) & 0xF8;

	return (unit == 8 ? RGBFB_CHUNKY
		  : depth == 15 && unit == 16 ? RGBFB_R5G5B5
		  : depth == 16 && unit == 16 ? RGBFB_R5G6B5
		  : unit == 24 ? RGBFB_R8G8B8
		  : unit == 32 ? RGBFB_A8R8G8B8
		  : RGBFB_NONE);
}


int graphics_init(bool mousecapture)
{
	graphics_subinit();

	if (!init_colors())
		return 0;
  
	return 1;
}

void graphics_leave(void)
{
	graphics_subshutdown();

	if(renderer != NULL) {
		SDL_DestroyRenderer(renderer);
	  renderer = NULL;
	}
	SDL_DestroyWindow(sdlWindow);
  sdlWindow = NULL;
  vc_dispmanx_vsync_callback(dispmanxdisplay, NULL, NULL);
	vc_dispmanx_display_close(dispmanxdisplay);
	bcm_host_deinit();
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
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL,
		NULL,
		NULL);
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

	png_set_IHDR(png_ptr,
		info_ptr,
		w,
		h,
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	unsigned char *b = writeBuffer;

	int sizeX = w;
	int sizeY = h;
	int y;
	int x;

	unsigned short *p = (unsigned short *)pix;
	for (y = 0; y < sizeY; y++) 
	{
		for (x = 0; x < sizeX; x++) 
		{
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

	if (current_screenshot != NULL)
	{
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
	if (current_screenshot != NULL)
	{
		ret = save_png(current_screenshot, path);
		SDL_FreeSurface(current_screenshot);
		current_screenshot = NULL;
	}
	return ret;
}


static int currVSyncRate = 0;
bool vsync_switchmode(int hz)
{
	int changed_height = changed_prefs.gfx_monitor.gfx_size.height;
	struct amigadisplay *ad = &adisplays;
	
	if (hz >= 55)
		hz = 60;
	else
		hz = 50;

	if (hz == 50 && currVSyncRate == 60)
	{
	  // Switch from NTSC -> PAL
		switch (changed_height) {
		case 200: changed_height = 240; break;
		case 216: changed_height = 262; break;
		case 240: changed_height = 270; break;
		case 256: changed_height = 270; break;
		case 262: changed_height = 270; break;
		case 270: changed_height = 270; break;
		}
	}
	else if (hz == 60 && currVSyncRate == 50)
	{
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

  if(hz != currVSyncRate) 
  {
    currVSyncRate = hz;
  	black_screen_now();
    fpscounter_reset();
    time_per_frame = 1000 * 1000 / (hz);

    if(hz == host_hz)
      vsync_modulo = 1;
    else if (hz > host_hz)
      vsync_modulo = 6; // Amiga draws 6 frames while host has 5 vsyncs -> sync every 6th Amiga frame
    else
      vsync_modulo = 5; // Amiga draws 5 frames while host has 6 vsyncs -> sync every 5th Amiga frame
  }
  
  if(!ad->picasso_on && !ad->picasso_requested_on)
  	changed_prefs.gfx_monitor.gfx_size.height = changed_height;

	return true;
}

bool target_graphics_buffer_update(void)
{
	bool rate_changed = false;
  
	if (currprefs.gfx_monitor.gfx_size.height != changed_prefs.gfx_monitor.gfx_size.height)
	{
		update_display(&changed_prefs);
		rate_changed = true;
	}

	if (rate_changed)
	{
		black_screen_now();
		fpscounter_reset();
		time_per_frame = 1000 * 1000 / (currprefs.chipset_refreshrate);
	}

	return true;
}

#ifdef PICASSO96


int picasso_palette(struct MyCLUTEntry *CLUT)
{
	int changed = 0;
	
	for (int i = 0; i < 256; i++) {
    int r = CLUT[i].Red;
    int g = CLUT[i].Green;
    int b = CLUT[i].Blue;
		int value = (r << 16 | g << 8 | b);
		uae_u32 v = CONVERT_RGB(value);
		if (v !=  clut[i]) {
			clut[i] = v;
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
			int rgbFormat = (bitdepth == 8 ? RGBFB_CLUT : (bitdepth == 16 ? RGBFB_R5G6B5 : RGBFB_A8R8G8B8));
			int pixelFormat = 1 << rgbFormat;
			pixelFormat |= RGBFF_CHUNKY;
      
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
	if (screen_is_picasso)
	{
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
  if(dorender)
  {
    render_screen();
    show_screen(0);
  }
}

#endif // PICASSO96
