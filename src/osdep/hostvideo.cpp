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
 * The sound plays along with Paula's, as the MHI decoder's does (mhi.cpp):
 * its samples are taken at its own rate in emulated time, so picture and
 * sound keep together. When the decoder falls behind, the samples missed
 * are left out once they come, so that the sound does not lag.
 *
 * The decoding: FFmpeg's libraries on the Linux PC (HOSTVIDEO_FFMPEG), the
 * device's decoders through MediaCodec on Android (HOSTVIDEO_MEDIACODEC).
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "traps.h"
#include "filesys.h"
#include "devices.h"
#include "audio.h"
#include "native2amiga_api.h"
#include "resampler.h"
#include "hostvideo.h"

#ifdef HOSTVIDEO_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#endif
#ifdef HOSTVIDEO_MEDIACODEC
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
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

/* Decoded frames waiting to fall due; more when the sound is short */
#define QUEUE_FRAMES 8
#define QUEUE_FRAMES_MAX 64

/* Decoded stereo samples waiting to be played: a power of two, so indexes
   wrap by mask. The decoder keeps half a second ready. */
#define AUDIO_FRAMES (1 << 17)

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
	/* The rate of the sound, 0 for none. */
	int audio_rate;
	/* Takes up to n stereo samples, from the start of the video on;
	   fewer when the decoder is behind. */
	virtual int audio_read(uae_s16 (*out)[2], int n) = 0;
};

/* What the decoders share: a thread that decodes ahead into a queue of
   frames and a ring of sound, which the emulator's thread takes from. */
class decoding_source : public video_source {
public:
	bool take(double t);
	bool ended();
	int audio_read(uae_s16 (*out)[2], int n);

protected:
	decoding_source() { dropped = 0; audio_rate = 0; }
	~decoding_source() { delete[] audio; }
	/* Decodes a little; false at the end of the stream. */
	virtual bool decode_some() = 0;
	virtual void free_frame(void *f) = 0;
	void start_thread();
	/* First thing in the destructor of a decoder, as the thread uses its
	   parts; frees the frames too. */
	void stop_thread();
	/* pts in seconds, below 0 for none */
	void queue_frame(double pts, void *f);
	size_t queued();
	/* The ring for the sound, at its rate. */
	void audio_open(int rate);
	/* Sound that starts after the picture waits, with silence; to be told
	   the time stamp of the first sound. */
	void audio_start(double pts);
	void audio_write(const uae_s16 *in, int n);

	/* the time stamp of the start of the video, below 0 until known */
	double start = -1;
	/* the frame shown, the emulator thread's own */
	void *current = NULL;

private:
	void decode_thread();
	bool want_more();

	std::thread thread;
	std::mutex lock;
	std::condition_variable wake;
	bool stop = false;
	bool eof = false;
	struct frame { double pts; void *f; };
	std::deque<frame> queue;

	bool audio_started = false;
	/* written by the decoder's thread, read by the emulator's */
	uae_s16 (*audio)[2] = NULL;
	std::atomic<unsigned int> audio_write_pos{0}, audio_read_pos{0};
};

void decoding_source::start_thread()
{
	thread = std::thread(&decoding_source::decode_thread, this);
}

void decoding_source::stop_thread()
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
		free_frame(q.f);
	queue.clear();
	if (current)
		free_frame(current);
	current = NULL;
}

void decoding_source::queue_frame(double pts, void *f)
{
	std::unique_lock<std::mutex> g(lock);
	/* without a time stamp, right after the one before */
	if (pts < 0)
		pts = queue.empty() ? 0 : queue.back().pts;
	if (start < 0)
		start = pts;
	queue.push_back({ pts, f });
}

size_t decoding_source::queued()
{
	std::lock_guard<std::mutex> g(lock);
	return queue.size();
}

void decoding_source::audio_open(int rate)
{
	audio = new uae_s16[AUDIO_FRAMES][2];
	audio_rate = rate;
}

void decoding_source::audio_start(double pts)
{
	if (audio_started)
		return;
	audio_started = true;
	/* sound from before the picture's start is left out by the player */
	if (pts >= 0 && start >= 0) {
		int pad = (int)((pts - start) * audio_rate);
		if (pad > 0 && pad < AUDIO_FRAMES / 2)
			audio_write(NULL, pad);
	}
}

void decoding_source::audio_write(const uae_s16 *in, int n)
{
	unsigned int w = audio_write_pos.load(std::memory_order_relaxed);
	for (int i = 0; i < n; i++, w++) {
		audio[w & (AUDIO_FRAMES - 1)][0] = in ? in[i * 2] : 0;
		audio[w & (AUDIO_FRAMES - 1)][1] = in ? in[i * 2 + 1] : 0;
	}
	audio_write_pos.store(w, std::memory_order_release);
}

/* Whether to decode on: a few frames of the picture and half a second of
   sound ready, within what the queues hold. Called with the lock held. */
bool decoding_source::want_more()
{
	if (queue.size() >= QUEUE_FRAMES_MAX)
		return false;
	if (!audio)
		return queue.size() < QUEUE_FRAMES;
	unsigned int fill = audio_write_pos.load(std::memory_order_relaxed) -
		audio_read_pos.load(std::memory_order_acquire);
	if (fill > AUDIO_FRAMES - 16384)
		return false;
	return queue.size() < QUEUE_FRAMES || fill < (unsigned int)audio_rate / 2;
}

void decoding_source::decode_thread()
{
	for (;;) {
		{
			std::unique_lock<std::mutex> g(lock);
			/* the sound is used up without a word to here, so look again
			   now and then */
			wake.wait_for(g, std::chrono::milliseconds(10), [this] { return stop || want_more(); });
			if (stop)
				return;
			if (!want_more())
				continue;
		}
		if (!decode_some()) {
			std::lock_guard<std::mutex> g(lock);
			eof = true;
			return;
		}
	}
}

bool decoding_source::take(double t)
{
	void *latest = NULL;
	{
		std::lock_guard<std::mutex> g(lock);
		if (queue.empty())
			return false;
		while (!queue.empty() && queue.front().pts - start <= t) {
			if (latest) {
				free_frame(latest);
				dropped++;
			}
			latest = queue.front().f;
			queue.pop_front();
		}
	}
	if (!latest)
		return false;
	wake.notify_all();
	if (current)
		free_frame(current);
	current = latest;
	return true;
}

bool decoding_source::ended()
{
	std::lock_guard<std::mutex> g(lock);
	if (!eof || !queue.empty())
		return false;
	return !audio || audio_write_pos.load(std::memory_order_acquire) ==
		audio_read_pos.load(std::memory_order_relaxed);
}

int decoding_source::audio_read(uae_s16 (*out)[2], int n)
{
	if (!audio)
		return 0;
	unsigned int r = audio_read_pos.load(std::memory_order_relaxed);
	unsigned int fill = audio_write_pos.load(std::memory_order_acquire) - r;
	if ((unsigned int)n > fill)
		n = fill;
	for (int i = 0; i < n; i++, r++) {
		out[i][0] = audio[r & (AUDIO_FRAMES - 1)][0];
		out[i][1] = audio[r & (AUDIO_FRAMES - 1)][1];
	}
	audio_read_pos.store(r, std::memory_order_release);
	return n;
}

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

/* FFmpeg's libraries, on the Linux PC */
class ffmpeg_source : public decoding_source {
public:
	~ffmpeg_source();
	bool open(const char *path);
	void render(uae_u8 *dst, int w, int h, int format);

protected:
	bool decode_some();
	void free_frame(void *f) { AVFrame *a = (AVFrame *)f; av_frame_free(&a); }

private:
	bool open_audio();
	void queue_audio(AVFrame *f);

	AVFormatContext *fmt = NULL;
	AVCodecContext *dec = NULL;
	int stream = -1;
	double time_base = 0;
	SwsContext *sws = NULL;

	int audio_stream = -1;
	AVCodecContext *adec = NULL;
	double audio_time_base = 0;
	SwrContext *swr = NULL;
};

bool ffmpeg_source::open(const char *path)
{
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
	if (st->start_time != AV_NOPTS_VALUE)
		start = st->start_time * time_base;
	if (!open_audio())
		write_log(_T("HostPlay: %s plays without sound\n"), path);
	width = dec->width;
	height = dec->height;
	/* pixels that are not square: wider or narrower to show */
	AVRational sar = av_guess_sample_aspect_ratio(fmt, st, NULL);
	if (sar.num > 0 && sar.den > 0 && sar.num != sar.den)
		width = (int)((double)width * sar.num / sar.den + 0.5);
	if (width <= 0 || height <= 0 || width > 0xffff || height > 0xffff)
		return false;
	write_log(_T("HostPlay: %s, %s %dx%d\n"), path, codec->name, width, height);
	start_thread();
	return true;
}

/* The sound that goes with the picture, as 16-bit stereo at its own rate */
bool ffmpeg_source::open_audio()
{
	const AVCodec *codec = NULL;
	audio_stream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, stream, &codec, 0);
	if (audio_stream < 0 || !codec)
		return false;
	AVStream *st = fmt->streams[audio_stream];
	adec = avcodec_alloc_context3(codec);
	if (!adec || avcodec_parameters_to_context(adec, st->codecpar) < 0 || avcodec_open2(adec, codec, NULL) < 0) {
		audio_stream = -1;
		return false;
	}
	AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
	if (swr_alloc_set_opts2(&swr, &stereo, AV_SAMPLE_FMT_S16, adec->sample_rate,
			&adec->ch_layout, adec->sample_fmt, adec->sample_rate, 0, NULL) < 0 || swr_init(swr) < 0) {
		audio_stream = -1;
		return false;
	}
	audio_time_base = av_q2d(st->time_base);
	/* the earlier of the two starts is the start of the video */
	if (st->start_time != AV_NOPTS_VALUE) {
		double t = st->start_time * audio_time_base;
		if (start < 0 || t < start)
			start = t;
	}
	audio_open(adec->sample_rate);
	write_log(_T("HostPlay: sound %s, %d Hz\n"), codec->name, audio_rate);
	return true;
}

ffmpeg_source::~ffmpeg_source()
{
	stop_thread();
	swr_free(&swr);
	avcodec_free_context(&adec);
	sws_freeContext(sws);
	avcodec_free_context(&dec);
	avformat_close_input(&fmt);
}

void ffmpeg_source::queue_audio(AVFrame *f)
{
	int64_t ts = f->best_effort_timestamp;
	audio_start(ts == AV_NOPTS_VALUE ? -1 : ts * audio_time_base);
	/* the same rate in and out: as many samples, and a few it held back */
	int room = f->nb_samples + 64;
	std::vector<uae_s16> buf((size_t)room * 2);
	uint8_t *out = (uint8_t *)buf.data();
	int got = swr_convert(swr, &out, room, (const uint8_t **)f->extended_data, f->nb_samples);
	if (got > 0)
		audio_write(buf.data(), got);
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
			/* the end: what the decoders still hold */
			avcodec_send_packet(dec, NULL);
			if (adec)
				avcodec_send_packet(adec, NULL);
			more = false;
		} else if (pkt->stream_index == stream) {
			avcodec_send_packet(dec, pkt);
		} else if (pkt->stream_index == audio_stream) {
			avcodec_send_packet(adec, pkt);
		}
		av_packet_unref(pkt);
		while (avcodec_receive_frame(dec, f) == 0) {
			int64_t ts = f->best_effort_timestamp;
			queue_frame(ts == AV_NOPTS_VALUE ? -1 : ts * time_base, av_frame_clone(f));
			av_frame_unref(f);
			got = true;
		}
		while (adec && avcodec_receive_frame(adec, f) == 0) {
			queue_audio(f);
			av_frame_unref(f);
			got = true;
		}
	}
	av_frame_free(&f);
	av_packet_free(&pkt);
	return more;
}

void ffmpeg_source::render(uae_u8 *dst, int w, int h, int format)
{
	AVFrame *cur = (AVFrame *)current;
	if (!cur) {
		memset(dst, 0, (size_t)w * h * format_bpp(format));
		return;
	}
	enum AVPixelFormat out = ffmpeg_format(format);
	sws = sws_getCachedContext(sws, cur->width, cur->height, (enum AVPixelFormat)cur->format,
		w, h, out, SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws)
		return;
	uint8_t *planes[4] = { dst, NULL, NULL, NULL };
	int strides[4] = { w * format_bpp(format), 0, 0, 0 };
	sws_scale(sws, cur->data, cur->linesize, 0, cur->height, planes, strides);
}

#endif /* HOSTVIDEO_FFMPEG */

#ifdef HOSTVIDEO_MEDIACODEC

/* One pixel in the screen's format, as it lies in the Amiga's memory: the
   PC formats are little endian words. */
static inline void put_pixel(uae_u8 *p, int format, int r, int g, int b)
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

static inline int clamp255(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/* A decoded picture, 4:2:0: the luma plane, and the chroma in planes of
   their own or interleaved, as the decoder gave them. */
struct yuv_frame {
	int w, h;
	std::vector<uae_u8> y;
	std::vector<uae_u8> c;
	int u_off, v_off, c_stride, c_step;
};

/* Scales a picture to w x h in a Picasso96 RGBFormat: luma bilinear,
   chroma from the nearest sample. */
static void yuv_render(const yuv_frame *f, uae_u8 *dst, int w, int h, int format, bool bt709, bool full_range)
{
	/* the colour matrix, in 1/1024 */
	int ky, kvr, kug, kvg, kub;
	if (bt709) {
		ky = full_range ? 1024 : 1192; kvr = 1836; kug = 218; kvg = 546; kub = 2163;
	} else {
		ky = full_range ? 1024 : 1192; kvr = 1634; kug = 401; kvg = 832; kub = 2066;
	}
	if (full_range) {
		/* the same matrix, without the scaling of the chroma */
		kvr = kvr * 1024 / 1170; kug = kug * 1024 / 1170;
		kvg = kvg * 1024 / 1170; kub = kub * 1024 / 1170;
	}
	int y0 = full_range ? 0 : 16;
	int bpp = format_bpp(format);
	std::vector<int> xs(w), xf(w);
	for (int x = 0; x < w; x++) {
		/* the sample under the middle of the pixel, in 1/256 */
		int sx = (int)(((2 * x + 1) * (int64_t)f->w * 128) / w) - 128;
		if (sx < 0)
			sx = 0;
		xs[x] = sx >> 8;
		xf[x] = sx & 255;
		if (xs[x] >= f->w - 1) {
			xs[x] = f->w - 1;
			xf[x] = 0;
		}
	}
	for (int y = 0; y < h; y++) {
		int sy = (int)(((2 * y + 1) * (int64_t)f->h * 128) / h) - 128;
		if (sy < 0)
			sy = 0;
		int ys = sy >> 8, yf = sy & 255;
		if (ys >= f->h - 1) {
			ys = f->h - 1;
			yf = 0;
		}
		const uae_u8 *l0 = &f->y[(size_t)ys * f->w];
		const uae_u8 *l1 = yf ? l0 + f->w : l0;
		const uae_u8 *cl = &f->c[(size_t)(ys / 2) * f->c_stride];
		uae_u8 *p = dst + (size_t)y * w * bpp;
		for (int x = 0; x < w; x++, p += bpp) {
			int xi = xs[x], xw = xf[x];
			int xi1 = xw ? xi + 1 : xi;
			int top = l0[xi] * (256 - xw) + l0[xi1] * xw;
			int bot = l1[xi] * (256 - xw) + l1[xi1] * xw;
			int lum = ((top * (256 - yf) + bot * yf) >> 16) - y0;
			const uae_u8 *c = cl + (xi / 2) * f->c_step;
			int u = c[f->u_off] - 128, v = c[f->v_off] - 128;
			int yy = lum * ky;
			put_pixel(p, format,
				clamp255((yy + kvr * v) >> 10),
				clamp255((yy - kug * u - kvg * v) >> 10),
				clamp255((yy + kub * u) >> 10));
		}
	}
}

/* MediaCodec's colour formats */
#define COLOR_YUV420_PLANAR 19
#define COLOR_YUV420_SEMIPLANAR 21
#define COLOR_YUV420_FLEXIBLE 0x7f420888
#define COLOR_QCOM_SEMIPLANAR_32M 0x7fa30c04

/* The device's decoders through MediaCodec, on Android: in hardware where
   the device has them. */
class mediacodec_source : public decoding_source {
public:
	~mediacodec_source();
	bool open(const char *path);
	void render(uae_u8 *dst, int w, int h, int format);

protected:
	bool decode_some();
	void free_frame(void *f) { delete (yuv_frame *)f; }

private:
	bool feed();
	bool drain_video();
	bool drain_audio();
	void video_format();

	int fd = -1;
	AMediaExtractor *ex = NULL;
	AMediaCodec *vcodec = NULL, *acodec = NULL;
	int vtrack = -1, atrack = -1;
	bool input_done = false, veos_sent = false, aeos_sent = false;
	bool video_done = false, audio_done = false;

	/* the decoder's pictures */
	int pic_w = 0, pic_h = 0, stride = 0, slice_height = 0, color = 0;
	int crop_left = 0, crop_top = 0, crop_right = -1, crop_bottom = -1;
	bool bt709 = false, full_range = false;

	int achannels = 2;
	bool afloat = false;
	std::vector<uae_s16> abuf;
};

bool mediacodec_source::open(const char *path)
{
	fd = ::open(path, O_RDONLY);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) < 0)
		return false;
	ex = AMediaExtractor_new();
	if (!ex || AMediaExtractor_setDataSourceFd(ex, fd, 0, st.st_size) != AMEDIA_OK)
		return false;
	AMediaFormat *vfmt = NULL, *afmt = NULL;
	const char *vmime = NULL, *amime = NULL;
	size_t tracks = AMediaExtractor_getTrackCount(ex);
	for (size_t i = 0; i < tracks; i++) {
		AMediaFormat *f = AMediaExtractor_getTrackFormat(ex, i);
		const char *mime = NULL;
		if (f && AMediaFormat_getString(f, AMEDIAFORMAT_KEY_MIME, &mime)) {
			if (vtrack < 0 && !strncmp(mime, "video/", 6)) {
				vtrack = (int)i;
				vfmt = f;
				vmime = mime;
				continue;
			}
			if (atrack < 0 && !strncmp(mime, "audio/", 6)) {
				atrack = (int)i;
				afmt = f;
				amime = mime;
				continue;
			}
		}
		if (f)
			AMediaFormat_delete(f);
	}
	bool ok = false;
	if (vtrack >= 0) {
		int32_t w = 0, h = 0, sw = 0, sh = 0;
		AMediaFormat_getInt32(vfmt, AMEDIAFORMAT_KEY_WIDTH, &w);
		AMediaFormat_getInt32(vfmt, AMEDIAFORMAT_KEY_HEIGHT, &h);
		width = w;
		height = h;
		/* pixels that are not square: wider or narrower to show */
		if (AMediaFormat_getInt32(vfmt, "sar-width", &sw) && AMediaFormat_getInt32(vfmt, "sar-height", &sh) &&
				sw > 0 && sh > 0 && sw != sh)
			width = (int)((double)w * sw / sh + 0.5);
		vcodec = AMediaCodec_createDecoderByType(vmime);
		/* pictures in memory, in one of the YUV 4:2:0 layouts */
		AMediaFormat_setInt32(vfmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_YUV420_FLEXIBLE);
		if (vcodec && AMediaCodec_configure(vcodec, vfmt, NULL, NULL, 0) == AMEDIA_OK &&
				AMediaCodec_start(vcodec) == AMEDIA_OK) {
			AMediaExtractor_selectTrack(ex, vtrack);
			write_log(_T("HostPlay: %s, %s %dx%d\n"), path, vmime, width, height);
			ok = true;
		} else {
			write_log(_T("HostPlay: no decoder for %s\n"), vmime);
		}
	}
	if (ok && atrack >= 0) {
		acodec = AMediaCodec_createDecoderByType(amime);
		if (acodec && AMediaCodec_configure(acodec, afmt, NULL, NULL, 0) == AMEDIA_OK &&
				AMediaCodec_start(acodec) == AMEDIA_OK) {
			AMediaExtractor_selectTrack(ex, atrack);
		} else {
			write_log(_T("HostPlay: no decoder for %s, plays without sound\n"), amime);
			if (acodec)
				AMediaCodec_delete(acodec);
			acodec = NULL;
		}
	}
	if (vfmt)
		AMediaFormat_delete(vfmt);
	if (afmt)
		AMediaFormat_delete(afmt);
	if (!ok || width <= 0 || height <= 0 || width > 0xffff || height > 0xffff)
		return false;
	/* The rate of the sound comes with the decoder's first output, which can
	   differ from the file's: decoded up to there before the player starts
	   its clock. */
	for (int i = 0; i < 500 && acodec && !audio_rate && !audio_done && queued() < QUEUE_FRAMES_MAX; i++) {
		if (!decode_some())
			break;
	}
	if (acodec && !audio_rate)
		write_log(_T("HostPlay: no sound came out\n"));
	start_thread();
	return true;
}

mediacodec_source::~mediacodec_source()
{
	stop_thread();
	if (vcodec) {
		AMediaCodec_stop(vcodec);
		AMediaCodec_delete(vcodec);
	}
	if (acodec) {
		AMediaCodec_stop(acodec);
		AMediaCodec_delete(acodec);
	}
	if (ex)
		AMediaExtractor_delete(ex);
	if (fd >= 0)
		close(fd);
}

/* Gives a decoder the next piece of the file; false when it has no room. */
bool mediacodec_source::feed()
{
	if (input_done) {
		/* the end of the stream, for each decoder once */
		if (!veos_sent) {
			ssize_t i = AMediaCodec_dequeueInputBuffer(vcodec, 2000);
			if (i >= 0) {
				AMediaCodec_queueInputBuffer(vcodec, i, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
				veos_sent = true;
			}
		}
		if (acodec && !aeos_sent) {
			ssize_t i = AMediaCodec_dequeueInputBuffer(acodec, 2000);
			if (i >= 0) {
				AMediaCodec_queueInputBuffer(acodec, i, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
				aeos_sent = true;
			}
		}
		return false;
	}
	int track = AMediaExtractor_getSampleTrackIndex(ex);
	if (track < 0) {
		input_done = true;
		return true;
	}
	AMediaCodec *codec = track == vtrack ? vcodec : (track == atrack ? acodec : NULL);
	if (!codec) {
		AMediaExtractor_advance(ex);
		return true;
	}
	ssize_t i = AMediaCodec_dequeueInputBuffer(codec, 2000);
	if (i < 0)
		return false;
	size_t size;
	uint8_t *buf = AMediaCodec_getInputBuffer(codec, i, &size);
	ssize_t n = AMediaExtractor_readSampleData(ex, buf, size);
	int64_t pts = AMediaExtractor_getSampleTime(ex);
	AMediaCodec_queueInputBuffer(codec, i, 0, n < 0 ? 0 : n, pts < 0 ? 0 : pts, 0);
	AMediaExtractor_advance(ex);
	return true;
}

void mediacodec_source::video_format()
{
	AMediaFormat *f = AMediaCodec_getOutputFormat(vcodec);
	if (!f)
		return;
	AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_WIDTH, &pic_w);
	AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_HEIGHT, &pic_h);
	stride = pic_w;
	slice_height = pic_h;
	AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_STRIDE, &stride);
	AMediaFormat_getInt32(f, "slice-height", &slice_height);
	AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color);
	crop_left = crop_top = 0;
	crop_right = pic_w - 1;
	crop_bottom = pic_h - 1;
	AMediaFormat_getInt32(f, "crop-left", &crop_left);
	AMediaFormat_getInt32(f, "crop-top", &crop_top);
	AMediaFormat_getInt32(f, "crop-right", &crop_right);
	AMediaFormat_getInt32(f, "crop-bottom", &crop_bottom);
	int32_t standard = 0, range = 0;
	/* without word of it, HD is BT.709 and the rest BT.601 */
	bt709 = pic_h >= 720;
	if (AMediaFormat_getInt32(f, "color-standard", &standard))
		bt709 = standard == 1;
	full_range = AMediaFormat_getInt32(f, "color-range", &range) && range == 1;
	if (stride < pic_w)
		stride = pic_w;
	if (slice_height < pic_h)
		slice_height = pic_h;
	write_log(_T("HostPlay: pictures %dx%d, colour format %x, stride %d, slice height %d, crop %d,%d-%d,%d\n"),
		pic_w, pic_h, color, stride, slice_height, crop_left, crop_top, crop_right, crop_bottom);
	AMediaFormat_delete(f);
}

/* Takes a picture out of the video decoder, if one is there. */
bool mediacodec_source::drain_video()
{
	AMediaCodecBufferInfo info;
	ssize_t i = AMediaCodec_dequeueOutputBuffer(vcodec, &info, 0);
	if (i == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		video_format();
		return true;
	}
	if (i < 0)
		return false;
	if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
		video_done = true;
	size_t size;
	uint8_t *buf = AMediaCodec_getOutputBuffer(vcodec, i, &size);
	if (!pic_w)
		video_format();
	int w = crop_right - crop_left + 1, h = crop_bottom - crop_top + 1;
	if (buf && info.size > 0 && w > 1 && h > 1) {
		buf += info.offset;
		/* where the chroma lies */
		int rows = slice_height;
		if (color == COLOR_QCOM_SEMIPLANAR_32M)
			rows = (rows + 31) & ~31;
		bool planar = color == COLOR_YUV420_PLANAR;
		size_t c_base = (size_t)stride * rows;
		int c_stride = planar ? stride / 2 : stride;
		size_t c_size = (size_t)c_stride * ((rows + 1) / 2);
		size_t need = c_base + (planar ? 2 * c_size : c_size);
		if (need <= (size_t)info.size) {
			yuv_frame *fr = new yuv_frame;
			fr->w = w & ~1;
			fr->h = h & ~1;
			fr->y.resize((size_t)fr->w * fr->h);
			for (int y = 0; y < fr->h; y++)
				memcpy(&fr->y[(size_t)y * fr->w], buf + (size_t)(crop_top + y) * stride + crop_left, fr->w);
			int cw = fr->w / 2, ch = fr->h / 2;
			if (planar) {
				/* U and V planes of their own: both go side by side */
				fr->c_stride = cw * 2;
				fr->c_step = 1;
				fr->u_off = 0;
				fr->v_off = cw;
				fr->c.resize((size_t)fr->c_stride * ch);
				for (int y = 0; y < ch; y++) {
					size_t row = (size_t)(crop_top / 2 + y) * c_stride + crop_left / 2;
					memcpy(&fr->c[(size_t)y * fr->c_stride], buf + c_base + row, cw);
					memcpy(&fr->c[(size_t)y * fr->c_stride + cw], buf + c_base + c_size + row, cw);
				}
			} else {
				/* U and V interleaved, NV12 */
				fr->c_stride = cw * 2;
				fr->c_step = 2;
				fr->u_off = 0;
				fr->v_off = 1;
				fr->c.resize((size_t)fr->c_stride * ch);
				for (int y = 0; y < ch; y++)
					memcpy(&fr->c[(size_t)y * fr->c_stride],
						buf + c_base + (size_t)(crop_top / 2 + y) * c_stride + (crop_left & ~1), cw * 2);
			}
			queue_frame(info.presentationTimeUs / 1000000.0, fr);
		}
	}
	AMediaCodec_releaseOutputBuffer(vcodec, i, false);
	return true;
}

/* Takes sound out of the audio decoder, if some is there. */
bool mediacodec_source::drain_audio()
{
	AMediaCodecBufferInfo info;
	ssize_t i = AMediaCodec_dequeueOutputBuffer(acodec, &info, 0);
	if (i == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		AMediaFormat *f = AMediaCodec_getOutputFormat(acodec);
		if (f) {
			int32_t rate = 0, channels = 0, encoding = 2;
			AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_SAMPLE_RATE, &rate);
			AMediaFormat_getInt32(f, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
			AMediaFormat_getInt32(f, "pcm-encoding", &encoding);
			AMediaFormat_delete(f);
			if (channels > 0)
				achannels = channels;
			afloat = encoding == 4;
			if (!audio_rate && rate > 0) {
				audio_open(rate);
				write_log(_T("HostPlay: sound %d Hz, %d channels\n"), rate, achannels);
			}
		}
		return true;
	}
	if (i < 0)
		return false;
	if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
		audio_done = true;
	size_t size;
	uint8_t *buf = AMediaCodec_getOutputBuffer(acodec, i, &size);
	int bytes = afloat ? 4 : 2;
	int n = info.size / (bytes * achannels);
	if (buf && n > 0 && audio_rate) {
		buf += info.offset;
		audio_start(info.presentationTimeUs / 1000000.0);
		abuf.resize((size_t)n * 2);
		/* the first two channels, or the one twice */
		int right = achannels > 1 ? 1 : 0;
		for (int s = 0; s < n; s++) {
			if (afloat) {
				const float *in = (const float *)buf + (size_t)s * achannels;
				abuf[s * 2] = (uae_s16)clamp16((int)(in[0] * 32767.0f));
				abuf[s * 2 + 1] = (uae_s16)clamp16((int)(in[right] * 32767.0f));
			} else {
				const uae_s16 *in = (const uae_s16 *)buf + (size_t)s * achannels;
				abuf[s * 2] = in[0];
				abuf[s * 2 + 1] = in[right];
			}
		}
		audio_write(abuf.data(), n);
	}
	AMediaCodec_releaseOutputBuffer(acodec, i, false);
	return true;
}

/* Feeds and drains the decoders until something came out, or the end.
   False at the end of the stream. */
bool mediacodec_source::decode_some()
{
	for (int tries = 0; tries < 50; tries++) {
		bool fed = feed();
		bool got = false;
		while (!video_done && drain_video())
			got = true;
		while (acodec && !audio_done && drain_audio())
			got = true;
		if (video_done && (!acodec || audio_done))
			return false;
		if (got)
			return true;
		/* nowhere to put the next piece: the decoders are busy */
		if (!fed)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return true;
}

void mediacodec_source::render(uae_u8 *dst, int w, int h, int format)
{
	yuv_frame *f = (yuv_frame *)current;
	if (!f || f->w < 2 || f->h < 2) {
		memset(dst, 0, (size_t)w * h * format_bpp(format));
		return;
	}
	yuv_render(f, dst, w, h, format, bt709, full_range);
}

#endif /* HOSTVIDEO_MEDIACODEC */

/* A source for the file, or NULL. */
static video_source *open_source(const char *path)
{
#if defined(HOSTVIDEO_FFMPEG)
	ffmpeg_source *s = new ffmpeg_source();
#elif defined(HOSTVIDEO_MEDIACODEC)
	mediacodec_source *s = new mediacodec_source();
#endif
#if defined(HOSTVIDEO_FFMPEG) || defined(HOSTVIDEO_MEDIACODEC)
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

	/* the sound: its clock, samples read ahead, and those that were not
	   there when due, to be left out */
	struct resampler rs;
	uae_s16 abuf[512][2];
	int apos, alen;
	int missed;
	/* samples of silence in a row, up to RS_HIST: past that the mixing is
	   left out */
	int quiet;
};

static struct hostvideo players[HOSTVIDEO_PLAYERS];

bool hostvideo_playing;

static bool sounding(struct hostvideo *v)
{
	return v->used && v->source->audio_rate > 0 && !v->ended;
}

static void update_playing(void)
{
	bool playing = false;
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		if (sounding(&players[i]))
			playing = true;
	}
	if (playing && !hostvideo_playing)
		audio_activate();
	hostvideo_playing = playing;
}

static void audio_fill(struct hostvideo *v)
{
	v->apos = v->alen = 0;
	while (v->missed > 0) {
		int n = v->source->audio_read(v->abuf, v->missed < 512 ? v->missed : 512);
		if (!n)
			return;
		v->missed -= n;
	}
	v->alen = v->source->audio_read(v->abuf, 512);
}

/* One sample of the sound's clock; silence while paused. */
static void audio_tick(struct hostvideo *v)
{
	float l = 0, r = 0;
	if (!v->paused) {
		if (v->apos == v->alen)
			audio_fill(v);
		if (v->apos < v->alen) {
			l = v->abuf[v->apos][0];
			r = v->abuf[v->apos][1];
			v->apos++;
		} else if (v->missed < v->source->audio_rate * 10) {
			v->missed++;
		}
	}
	if (l == 0 && r == 0) {
		if (v->quiet < RS_HIST)
			v->quiet++;
	} else {
		v->quiet = 0;
	}
	resampler_put(&v->rs, l, r);
}

/* Runs the sound up to the current emulated time. */
static void audio_catchup(struct hostvideo *v)
{
	int n = resampler_catchup(&v->rs, true);
	while (n-- > 0)
		audio_tick(v);
}

static void mix(int *l, int *r)
{
	*l = *r = 0;
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		struct hostvideo *v = &players[i];
		if (!sounding(v))
			continue;
		audio_catchup(v);
		if (v->quiet >= RS_HIST)
			continue;
		int vl, vr;
		resampler_output(&v->rs, &vl, &vr);
		*l += vl;
		*r += vr;
	}
}

void hostvideo_mix_stereo(uae_u32 *left, uae_u32 *right)
{
	int l, r;
	mix(&l, &r);
	*left = (uae_u16)clamp16((uae_s16)*left + l);
	*right = (uae_u16)clamp16((uae_s16)*right + r);
}

void hostvideo_mix_mono(uae_u32 *mono)
{
	int l, r;
	mix(&l, &r);
	*mono = (uae_u16)clamp16((uae_s16)*mono + (l + r) / 2);
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
	bool loud = false;
	for (int i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		struct hostvideo *v = &players[i];
		if (!v->used)
			continue;
		catchup(v);
		if (sounding(v)) {
			/* also without sound output, so that the sound is used up */
			audio_catchup(v);
			if (v->quiet < RS_HIST)
				loud = true;
		}
		/* once, until the tool takes the frame */
		if ((v->ready || v->ended) && !v->signalled && v->buffer) {
			v->signalled = true;
			uae_Signal(v->task, v->sigmask);
		}
	}
	update_playing();
	/* keeps Paula's output running, which the sound rides on */
	if (loud)
		audio_activate();
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
	v->apos = v->alen = v->missed = 0;
	v->quiet = RS_HIST;
	if (source->audio_rate > 0)
		resampler_start(&v->rs, source->audio_rate);
	update_playing();
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
		update_playing();
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
	update_playing();
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
