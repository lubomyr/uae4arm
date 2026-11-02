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
 * A thread of the host decodes a few frames ahead. Frames fall due by their
 * time stamps against emulated time, at the clock the sound keeps
 * (resampler.h). One that falls due before the tool took the last one
 * replaces it: a slow Amiga drops frames, it does not fall behind.
 *
 * The decoding: FFmpeg's libraries on the Linux PC (HOSTVIDEO_FFMPEG).
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "traps.h"
#include "filesys.h"
#include "devices.h"
#include "native2amiga_api.h"
#include "resampler.h"
#include "hostvideo.h"

#ifdef HOSTVIDEO_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#endif

#define HOSTVIDEO_PLAYERS 2

/* the calls of HostPlay */
#define HV_CMD_VERSION 0
#define HV_CMD_OPEN 1       /* task, signal mask, Lock of the file */
#define HV_CMD_CLOSE 2      /* handle */
#define HV_CMD_INFO 3       /* handle: width << 16 | height of the video */
#define HV_CMD_TARGET 4     /* handle, buffer, width << 16 | height, RGBFormat */
#define HV_CMD_FRAME 5      /* handle: HV_FRAME_... */
#define HV_CMD_PAUSE 6      /* handle: 1 when paused now */

#define HV_API_VERSION 1

/* what HV_CMD_OPEN gives back besides a handle */
#define HV_OPEN_BUSY 0
#define HV_OPEN_NOPATH 0xfffffffe   /* not on a folder drive */
#define HV_OPEN_CANNOT 0xffffffff   /* not a video the host plays */

/* what HV_CMD_FRAME gives back */
#define HV_FRAME_NONE 0
#define HV_FRAME_NEW 1
#define HV_FRAME_END 2

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

/* Decoded frames waiting to fall due */
#define QUEUE_FRAMES 8

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

/* Where the frames come from: one decoder of the host or another. */
class video_source {
public:
	virtual ~video_source() {}
	/* The size to show the video at. */
	int width, height;
	/* Takes the latest frame due by t seconds from the start, and drops
	   those before it; false when none is new. */
	virtual bool take(double t) = 0;
	/* Puts the frame taken last into dst, at w x h in a Picasso96
	   RGBFormat; black before the first. */
	virtual void render(uae_u8 *dst, int w, int h, int format) = 0;
	/* All frames taken. */
	virtual bool ended() = 0;
	/* Frames dropped by take(), for the log. */
	int dropped;
};

#ifdef HOSTVIDEO_FFMPEG

static enum AVPixelFormat ffmpeg_format(int format)
{
	switch (format) {
	case RGBFB_R8G8B8: return AV_PIX_FMT_RGB24;
	case RGBFB_B8G8R8: return AV_PIX_FMT_BGR24;
	case RGBFB_R5G6B5PC: return AV_PIX_FMT_RGB565LE;
	case RGBFB_R5G5B5PC: return AV_PIX_FMT_RGB555LE;
	case RGBFB_A8R8G8B8: return AV_PIX_FMT_ARGB;
	case RGBFB_A8B8G8R8: return AV_PIX_FMT_ABGR;
	case RGBFB_R8G8B8A8: return AV_PIX_FMT_RGBA;
	case RGBFB_B8G8R8A8: return AV_PIX_FMT_BGRA;
	case RGBFB_R5G6B5: return AV_PIX_FMT_RGB565BE;
	case RGBFB_R5G5B5: return AV_PIX_FMT_RGB555BE;
	case RGBFB_B5G6R5PC: return AV_PIX_FMT_BGR565LE;
	case RGBFB_B5G5R5PC: return AV_PIX_FMT_BGR555LE;
	}
	return AV_PIX_FMT_NONE;
}

class ffmpeg_source : public video_source {
public:
	~ffmpeg_source();
	bool open(const char *path);
	bool take(double t);
	void render(uae_u8 *dst, int w, int h, int format);
	bool ended();

private:
	void decode_thread();
	bool decode_some();
	void queue_frame(AVFrame *f);

	AVFormatContext *fmt = NULL;
	AVCodecContext *dec = NULL;
	int stream = -1;
	double time_base = 0;
	double start = -1;
	SwsContext *sws = NULL;

	std::thread thread;
	std::mutex lock;
	std::condition_variable wake;
	bool stop = false;
	bool eof = false;
	struct frame { double pts; AVFrame *f; };
	std::deque<frame> queue;
	/* the frame shown, the emulator thread's own */
	AVFrame *current = NULL;
};

bool ffmpeg_source::open(const char *path)
{
	dropped = 0;
	if (avformat_open_input(&fmt, path, NULL, NULL) < 0)
		return false;
	if (avformat_find_stream_info(fmt, NULL) < 0)
		return false;
	const AVCodec *codec = NULL;
	stream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
	if (stream < 0 || !codec)
		return false;
	AVStream *st = fmt->streams[stream];
	dec = avcodec_alloc_context3(codec);
	if (!dec || avcodec_parameters_to_context(dec, st->codecpar) < 0)
		return false;
	dec->thread_count = 0;
	if (avcodec_open2(dec, codec, NULL) < 0)
		return false;
	time_base = av_q2d(st->time_base);
	width = dec->width;
	height = dec->height;
	/* pixels that are not square: wider or narrower to show */
	AVRational sar = av_guess_sample_aspect_ratio(fmt, st, NULL);
	if (sar.num > 0 && sar.den > 0 && sar.num != sar.den)
		width = (int)((double)width * sar.num / sar.den + 0.5);
	if (width <= 0 || height <= 0 || width > 0xffff || height > 0xffff)
		return false;
	write_log(_T("HostPlay: %s, %s %dx%d\n"), path, codec->name, width, height);
	thread = std::thread(&ffmpeg_source::decode_thread, this);
	return true;
}

ffmpeg_source::~ffmpeg_source()
{
	if (thread.joinable()) {
		{
			std::lock_guard<std::mutex> g(lock);
			stop = true;
		}
		wake.notify_all();
		thread.join();
	}
	for (auto &q : queue)
		av_frame_free(&q.f);
	av_frame_free(&current);
	sws_freeContext(sws);
	avcodec_free_context(&dec);
	avformat_close_input(&fmt);
}

void ffmpeg_source::queue_frame(AVFrame *f)
{
	int64_t ts = f->best_effort_timestamp;
	double pts = ts == AV_NOPTS_VALUE ? -1 : ts * time_base;
	std::unique_lock<std::mutex> g(lock);
	/* without a time stamp, right after the one before */
	if (pts < 0)
		pts = queue.empty() ? 0 : queue.back().pts;
	queue.push_back({ pts, av_frame_clone(f) });
}

/* Reads and decodes until a frame or two came out, or the end. False at the
   end of the stream. */
bool ffmpeg_source::decode_some()
{
	AVPacket *pkt = av_packet_alloc();
	AVFrame *f = av_frame_alloc();
	bool more = true;
	bool got = false;
	while (!got && more) {
		int ret = av_read_frame(fmt, pkt);
		if (ret < 0) {
			/* the end: what the decoder still holds */
			avcodec_send_packet(dec, NULL);
			more = false;
		} else if (pkt->stream_index == stream) {
			avcodec_send_packet(dec, pkt);
		}
		av_packet_unref(pkt);
		while (avcodec_receive_frame(dec, f) == 0) {
			queue_frame(f);
			av_frame_unref(f);
			got = true;
		}
	}
	av_frame_free(&f);
	av_packet_free(&pkt);
	return more;
}

void ffmpeg_source::decode_thread()
{
	for (;;) {
		{
			std::unique_lock<std::mutex> g(lock);
			wake.wait(g, [this] { return stop || queue.size() < QUEUE_FRAMES; });
			if (stop)
				return;
		}
		if (!decode_some()) {
			std::lock_guard<std::mutex> g(lock);
			eof = true;
			return;
		}
	}
}

bool ffmpeg_source::take(double t)
{
	AVFrame *latest = NULL;
	{
		std::lock_guard<std::mutex> g(lock);
		if (queue.empty())
			return false;
		if (start < 0)
			start = queue.front().pts;
		while (!queue.empty() && queue.front().pts - start <= t) {
			if (latest) {
				av_frame_free(&latest);
				dropped++;
			}
			latest = queue.front().f;
			queue.pop_front();
		}
	}
	if (!latest)
		return false;
	wake.notify_all();
	av_frame_free(&current);
	current = latest;
	return true;
}

void ffmpeg_source::render(uae_u8 *dst, int w, int h, int format)
{
	if (!current) {
		memset(dst, 0, (size_t)w * h * format_bpp(format));
		return;
	}
	enum AVPixelFormat out = ffmpeg_format(format);
	sws = sws_getCachedContext(sws, current->width, current->height, (enum AVPixelFormat)current->format,
		w, h, out, SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws)
		return;
	uint8_t *planes[4] = { dst, NULL, NULL, NULL };
	int strides[4] = { w * format_bpp(format), 0, 0, 0 };
	sws_scale(sws, current->data, current->linesize, 0, current->height, planes, strides);
}

bool ffmpeg_source::ended()
{
	std::lock_guard<std::mutex> g(lock);
	return eof && queue.empty();
}

#endif /* HOSTVIDEO_FFMPEG */

/* A source for the file, or NULL. */
static video_source *open_source(const char *path)
{
#ifdef HOSTVIDEO_FFMPEG
	ffmpeg_source *s = new ffmpeg_source();
	if (s->open(path))
		return s;
	write_log(_T("HostPlay: %s does not play\n"), path);
	delete s;
#endif
	return NULL;
}

struct hostvideo {
	bool used;
	uaecptr task;
	uae_u32 sigmask;
	video_source *source;

	/* the tool's buffer: the inside of its window */
	uaecptr buffer;
	int target_w, target_h;
	int format, bpp;

	bool paused;
	/* the video's clock: emulated time played */
	uae_u32 last_cycles;
	uae_s64 played;
	/* a new frame or the end the tool has not seen yet, and it was told */
	bool ready, ended, signalled;

	int stat_shown;
	int stat_time;

	std::vector<uae_u8> pixels;
};

static struct hostvideo players[HOSTVIDEO_PLAYERS];

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
	v->source->render(v->pixels.data(), v->target_w, v->target_h, v->format);
	copy_to_amiga(v->buffer, v->pixels.data(), (uae_u32)v->pixels.size());
}

/* Runs the video's clock up to the current emulated time and takes the
   frame due by then. */
static void catchup(struct hostvideo *v)
{
	uae_u32 now = get_cycles();
	uae_u32 delta = now - v->last_cycles;
	v->last_cycles = now;
	if (v->paused || v->ended)
		return;
	double second = resampler_clock() * CYCLE_UNIT;
	/* after a pause of the emulator, not a run of frames at once */
	if (delta > second / 4)
		delta = (uae_u32)(second / 4);
	v->played += delta;
	double t = v->played / second;
	if (v->source->take(t))
		v->ready = true;
	else if (v->source->ended())
		v->ended = true;
	if ((int)t >= v->stat_time + 10) {
		write_log(_T("HostPlay: %d frames shown, %d dropped by the host\n"), v->stat_shown, v->source->dropped);
		v->stat_time = (int)t;
		v->stat_shown = 0;
		v->source->dropped = 0;
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
		if ((v->ready || v->ended) && !v->signalled && v->buffer) {
			v->signalled = true;
			uae_Signal(v->task, v->sigmask);
		}
	}
}

static void player_close(struct hostvideo *v)
{
	delete v->source;
	v->source = NULL;
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

static uae_u32 player_open(TrapContext *ctx, uaecptr task, uae_u32 sigmask, uae_u32 lock)
{
	int i;
	for (i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		if (!players[i].used)
			break;
	}
	if (i == HOSTVIDEO_PLAYERS)
		return HV_OPEN_BUSY;
	TCHAR path[MAX_DPATH];
	if (!lock || get_native_path(ctx, lock, path))
		return HV_OPEN_NOPATH;
	video_source *source = open_source(path);
	if (!source)
		return HV_OPEN_CANNOT;
	struct hostvideo *v = &players[i];
	v->used = true;
	v->task = task;
	v->sigmask = sigmask;
	v->source = source;
	v->buffer = 0;
	v->bpp = 0;
	v->paused = false;
	v->last_cycles = get_cycles();
	v->played = 0;
	v->ready = v->ended = v->signalled = false;
	v->stat_shown = v->stat_time = 0;
	return i + 1;
}

uae_u32 hostvideo_call(TrapContext *ctx, uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3, uae_u32 arg4)
{
	if (cmd == HV_CMD_VERSION)
		return HV_API_VERSION;
	if (cmd == HV_CMD_OPEN)
		return player_open(ctx, arg1, arg2, arg3);

	struct hostvideo *v = get_player(arg1);
	if (!v)
		return 0;
	catchup(v);
	switch (cmd) {
	case HV_CMD_CLOSE:
		player_close(v);
		return 1;
	case HV_CMD_INFO:
		return (v->source->width << 16) | v->source->height;
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
		v->signalled = false;
		if (v->ready) {
			render(v);
			v->ready = false;
			v->stat_shown++;
			return HV_FRAME_NEW;
		}
		return v->ended ? HV_FRAME_END : HV_FRAME_NONE;
	case HV_CMD_PAUSE:
		v->paused = !v->paused;
		return v->paused ? 1 : 0;
	}
	return 0;
}

static void hostvideo_reset(int hardreset)
{
	/* the tools and their buffers are gone */
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		if (players[i].used)
			player_close(&players[i]);
	}
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
