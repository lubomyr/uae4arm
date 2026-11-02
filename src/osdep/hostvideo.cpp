/*
 * uae4arm - video decoded by the host, shown in a window of the Amiga
 *
 * The HostPlay tool (amiga/HostPlay.s) opens a window on an RTG screen and
 * hands a buffer for its inside over through uaelib function 202. The host
 * puts each frame into that buffer, scaled to the window and in the screen's
 * own pixel format, and signals the tool, which blits it with
 * p96WritePixelArray(): the 68k only copies, and the layers clip the frame
 * where other windows lie over it.
 *
 * Frames fall due by emulated time, at the clock the sound keeps
 * (resampler.h). A frame that falls due before the tool took the last one
 * replaces it: a slow Amiga drops frames, it does not fall behind.
 *
 * For now the frames are a test picture that moves.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <vector>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "devices.h"
#include "native2amiga_api.h"
#include "resampler.h"
#include "hostvideo.h"

#define HOSTVIDEO_PLAYERS 2

/* the calls of HostPlay */
#define HV_CMD_VERSION 0
#define HV_CMD_OPEN 1       /* task, signal mask */
#define HV_CMD_CLOSE 2      /* handle */
#define HV_CMD_INFO 3       /* handle: width << 16 | height of the video */
#define HV_CMD_TARGET 4     /* handle, buffer, width << 16 | height, RGBFormat */
#define HV_CMD_FRAME 5      /* handle: 1 when a new frame went into the buffer */
#define HV_CMD_PAUSE 6      /* handle: 1 when paused now */

#define HV_API_VERSION 1

/* libraries/Picasso96.h */
#define RGBFB_R8G8B8 2
#define RGBFB_B8G8R8 3
#define RGBFB_R5G6B5PC 4
#define RGBFB_R5G5B5PC 5
#define RGBFB_A8R8G8B8 6
#define RGBFB_A8B8G8R8 7
#define RGBFB_R8G8B8A8 8
#define RGBFB_B8G8R8A8 9
#define RGBFB_R5G6B5 10
#define RGBFB_R5G5B5 11
#define RGBFB_B5G6R5PC 12
#define RGBFB_B5G5R5PC 13

#define TEST_WIDTH 320
#define TEST_HEIGHT 240
#define TEST_FPS 25

struct hostvideo {
	bool used;
	uaecptr task;
	uae_u32 sigmask;
	int width, height;

	/* the tool's buffer: the inside of its window */
	uaecptr buffer;
	int target_w, target_h;
	int format, bpp;

	bool paused;
	/* the clock of the frames */
	uae_u32 last_cycles;
	uae_s64 cycle_acc;
	int frame;
	/* a frame fell due that the tool has not taken yet, and it was told */
	bool ready, signalled;

	/* frames that fell due and that the tool took, for the log */
	int stat_due, stat_shown;

	std::vector<uae_u8> pixels;
};

static struct hostvideo players[HOSTVIDEO_PLAYERS];

static int format_bpp(int format)
{
	switch (format) {
	case RGBFB_R5G6B5PC: case RGBFB_R5G5B5PC: case RGBFB_R5G6B5: case RGBFB_R5G5B5:
	case RGBFB_B5G6R5PC: case RGBFB_B5G5R5PC:
		return 2;
	case RGBFB_R8G8B8: case RGBFB_B8G8R8:
		return 3;
	case RGBFB_A8R8G8B8: case RGBFB_A8B8G8R8: case RGBFB_R8G8B8A8: case RGBFB_B8G8R8A8:
		return 4;
	}
	return 0;
}

/* One pixel in the screen's format, as it lies in the Amiga's memory: the
   PC formats are little endian words. */
static void put_pixel(uae_u8 *p, int format, int r, int g, int b)
{
	uae_u16 w;
	switch (format) {
	case RGBFB_R5G6B5PC:
		w = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
		p[0] = w; p[1] = w >> 8;
		break;
	case RGBFB_R5G5B5PC:
		w = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
		p[0] = w; p[1] = w >> 8;
		break;
	case RGBFB_B5G6R5PC:
		w = ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
		p[0] = w; p[1] = w >> 8;
		break;
	case RGBFB_B5G5R5PC:
		w = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
		p[0] = w; p[1] = w >> 8;
		break;
	case RGBFB_R5G6B5:
		w = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
		p[0] = w >> 8; p[1] = w;
		break;
	case RGBFB_R5G5B5:
		w = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
		p[0] = w >> 8; p[1] = w;
		break;
	case RGBFB_R8G8B8:
		p[0] = r; p[1] = g; p[2] = b;
		break;
	case RGBFB_B8G8R8:
		p[0] = b; p[1] = g; p[2] = r;
		break;
	case RGBFB_A8R8G8B8:
		p[0] = 0xff; p[1] = r; p[2] = g; p[3] = b;
		break;
	case RGBFB_A8B8G8R8:
		p[0] = 0xff; p[1] = b; p[2] = g; p[3] = r;
		break;
	case RGBFB_R8G8B8A8:
		p[0] = r; p[1] = g; p[2] = b; p[3] = 0xff;
		break;
	case RGBFB_B8G8R8A8:
		p[0] = b; p[1] = g; p[2] = r; p[3] = 0xff;
		break;
	}
}

/* The test picture: colour bars that scroll, and a white square that
   bounces, at the size of the window. */
static void test_frame(struct hostvideo *v)
{
	static const uae_u8 bars[8][3] = {
		{ 255, 255, 255 }, { 255, 255, 0 }, { 0, 255, 255 }, { 0, 255, 0 },
		{ 255, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 0, 0, 0 }
	};
	int w = v->target_w, h = v->target_h;
	int box = (w < h ? w : h) / 6;
	if (box < 4)
		box = 4;
	int bx = (v->frame * 3) % (2 * (w - box > 0 ? w - box : 1));
	if (bx >= w - box)
		bx = 2 * (w - box) - bx;
	int by = (v->frame * 2) % (2 * (h - box > 0 ? h - box : 1));
	if (by >= h - box)
		by = 2 * (h - box) - by;
	int shift = v->frame * 2;
	for (int y = 0; y < h; y++) {
		uae_u8 *p = &v->pixels[(size_t)y * w * v->bpp];
		for (int x = 0; x < w; x++, p += v->bpp) {
			int r, g, b;
			if (x >= bx && x < bx + box && y >= by && y < by + box) {
				r = g = b = 255;
			} else if (y >= h - h / 8) {
				/* a grey ramp at the bottom, for the depth of the colours */
				r = g = b = x * 255 / (w > 1 ? w - 1 : 1);
			} else {
				const uae_u8 *c = bars[((x + shift) * 8 / w) % 8];
				r = c[0]; g = c[1]; b = c[2];
			}
			put_pixel(p, v->format, r, g, b);
		}
	}
}

static void copy_to_amiga(uaecptr dst, const uae_u8 *src, uae_u32 size)
{
	if (valid_address(dst, size)) {
		memcpy(get_real_address(dst), src, size);
	} else {
		for (uae_u32 i = 0; i < size; i++)
			put_byte(dst + i, src[i]);
	}
}

static void render(struct hostvideo *v)
{
	if (!v->buffer || !v->bpp)
		return;
	test_frame(v);
	copy_to_amiga(v->buffer, v->pixels.data(), (uae_u32)v->pixels.size());
}

/* Runs the clock of the frames up to the current emulated time. */
static void catchup(struct hostvideo *v)
{
	uae_u32 now = get_cycles();
	uae_u32 delta = now - v->last_cycles;
	v->last_cycles = now;
	if (v->paused)
		return;
	uae_s64 frame_time = (uae_s64)(resampler_clock() * CYCLE_UNIT / TEST_FPS);
	/* after a pause of the emulator, not a run of frames at once */
	if (delta > frame_time * 4)
		delta = (uae_u32)(frame_time * 4);
	v->cycle_acc += delta;
	while (v->cycle_acc >= frame_time) {
		v->cycle_acc -= frame_time;
		v->frame++;
		v->ready = true;
		v->stat_due++;
	}
	if (v->stat_due >= TEST_FPS * 10) {
		write_log(_T("HostPlay: %d of %d frames shown\n"), v->stat_shown, v->stat_due);
		v->stat_due = v->stat_shown = 0;
	}
}

static void hostvideo_hsync(void)
{
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		struct hostvideo *v = &players[i];
		if (!v->used)
			continue;
		catchup(v);
		/* once, until the tool takes the frame */
		if (v->ready && !v->signalled && v->buffer) {
			v->signalled = true;
			uae_Signal(v->task, v->sigmask);
		}
	}
}

static void player_close(struct hostvideo *v)
{
	v->used = false;
	v->task = 0;
	v->buffer = 0;
	std::vector<uae_u8>().swap(v->pixels);
}

static struct hostvideo *get_player(uae_u32 handle)
{
	if (handle < 1 || handle > HOSTVIDEO_PLAYERS || !players[handle - 1].used)
		return NULL;
	return &players[handle - 1];
}

static uae_u32 player_open(uaecptr task, uae_u32 sigmask)
{
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		struct hostvideo *v = &players[i];
		if (v->used)
			continue;
		v->used = true;
		v->task = task;
		v->sigmask = sigmask;
		v->width = TEST_WIDTH;
		v->height = TEST_HEIGHT;
		v->buffer = 0;
		v->bpp = 0;
		v->paused = false;
		v->last_cycles = get_cycles();
		v->cycle_acc = 0;
		v->frame = 0;
		v->ready = v->signalled = false;
		v->stat_due = v->stat_shown = 0;
		write_log(_T("HostPlay: player %d for task %08x\n"), i + 1, task);
		return i + 1;
	}
	return 0;
}

uae_u32 hostvideo_call(uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3, uae_u32 arg4)
{
	if (cmd == HV_CMD_VERSION)
		return HV_API_VERSION;
	if (cmd == HV_CMD_OPEN)
		return player_open(arg1, arg2);

	struct hostvideo *v = get_player(arg1);
	if (!v)
		return 0;
	catchup(v);
	switch (cmd) {
	case HV_CMD_CLOSE:
		player_close(v);
		return 1;
	case HV_CMD_INFO:
		return (v->width << 16) | v->height;
	case HV_CMD_TARGET: {
		int w = arg3 >> 16, h = arg3 & 0xffff, bpp = format_bpp(arg4);
		v->buffer = 0;
		if (!arg2 || w <= 0 || h <= 0 || !bpp) {
			write_log(_T("HostPlay: no window of %dx%d in RGBFormat %d\n"), w, h, arg4);
			return 0;
		}
		v->buffer = arg2;
		v->target_w = w;
		v->target_h = h;
		v->format = arg4;
		v->bpp = bpp;
		v->pixels.resize((size_t)w * h * bpp);
		/* the frame of now, for the window to show at once */
		render(v);
		v->ready = v->signalled = false;
		return 1;
	}
	case HV_CMD_FRAME:
		if (!v->ready)
			return 0;
		render(v);
		v->ready = v->signalled = false;
		v->stat_shown++;
		return 1;
	case HV_CMD_PAUSE:
		v->paused = !v->paused;
		return v->paused ? 1 : 0;
	}
	return 0;
}

static void hostvideo_reset(int hardreset)
{
	/* the tools and their buffers are gone */
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++)
		player_close(&players[i]);
}

static void hostvideo_free(void)
{
	hostvideo_reset(1);
}

void hostvideo_install(void)
{
	device_add_hsync(hostvideo_hsync);
	device_add_reset(hostvideo_reset);
	device_add_exit(hostvideo_free);
}
