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
 * Pictures of the Amiga, IFF ILBM, it decodes itself on both.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <string>
#include <chrono>
#include <unistd.h>

#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "traps.h"
#include "filesys.h"
#include "autoconf.h"
#include "devices.h"
#include "audio.h"
#include "native2amiga_api.h"
#include "resampler.h"
#include "hostvideo.h"
#include "picasso96.h"

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
#include <dlfcn.h>
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
#define HV_CMD_TIME 7       /* handle: the time played, in ms */
#define HV_CMD_DURATION 8   /* handle: the length of the video in ms, 0 when unknown */
#define HV_CMD_SEEK 9       /* handle, time in ms */
#define HV_CMD_DESCRIBE 10  /* handle, buffer, its size: "(width x height) (codec)" */
#define HV_CMD_OPENCOPY 11  /* task, signal mask: plays the copy made through
                               uaelib function 200 (4, 5) of a file the host
                               does not reach */

#define HV_CMD_MODE 12      /* handle, n, width << 16 | height of the Workbench:
                               the n-th size for a screen of its own, best
                               first, as width << 16 | height; 0 past the last */
#define HV_CMD_FULLSCREEN 13 /* handle, 1 on a screen of its own, 0 in a window */

#define HV_API_VERSION 5     /* a screen of its own, for HostView */

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

static int format_bpp(int format);

/* Black: all the formats have it as zeros. */
static void clear(uae_u8 *dst, int w, int h, int stride, int format)
{
	for (int y = 0; y < h; y++)
		memset(dst + (size_t)y * stride, 0, (size_t)w * format_bpp(format));
}

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
	/* Its length in seconds, 0 when unknown, and what it is, for the title */
	double duration;
	char codec[32];
	/* Goes to t seconds from the start, in the background: the frames that
	   come after are of there. */
	virtual void seek(double t) = 0;
	/* Takes the latest frame due by t seconds from the start, and drops
	   those before it; false when none is new. */
	virtual bool take(double t) = 0;
	/* Puts the frame taken last into dst, at w x h in a Picasso96
	   RGBFormat, rows stride bytes apart; black before the first. */
	virtual void render(uae_u8 *dst, int w, int h, int stride, int format) = 0;
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
	void seek(double t);
	bool take(double t);
	bool ended();
	int audio_read(uae_s16 (*out)[2], int n);

protected:
	decoding_source() { dropped = 0; audio_rate = 0; duration = 0; codec[0] = 0; }
	~decoding_source() { delete[] audio; }
	/* Decodes a little; false at the end of the stream. */
	virtual bool decode_some() = 0;
	virtual void free_frame(void *f) = 0;
	/* Moves the reading to the key frame at or before pts and empties the
	   decoders; in the decoding thread. */
	virtual void seek_to(double pts) = 0;
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
	void halt_thread();

	std::thread thread;
	std::mutex lock;
	std::condition_variable wake;
	bool stop = false;
	bool eof = false;
	bool running = false;
	struct frame { double pts; void *f; };
	std::deque<frame> queue;

	/* A seek the thread is yet to make: where to, as a time stamp and as
	   seconds from the start. Each seek starts a new generation; what the
	   thread decoded for an older one is left out. */
	double seek_pts = -1, seek_t = 0;
	std::atomic<unsigned int> generation{0};
	unsigned int thread_generation = 0;
	/* where the sound of the last seek starts in the ring: what lies
	   before it the emulator's side skips */
	std::atomic<unsigned int> audio_floor{0};

	bool audio_started = false;
	/* the time the sound in the ring starts at, and samples to leave out
	   of it yet */
	double audio_origin = 0;
	int audio_skip = 0;
	/* written by the decoder's thread, read by the emulator's */
	uae_s16 (*audio)[2] = NULL;
	std::atomic<unsigned int> audio_write_pos{0}, audio_read_pos{0};
};

void decoding_source::start_thread()
{
	running = true;
	thread = std::thread(&decoding_source::decode_thread, this);
}

void decoding_source::halt_thread()
{
	if (thread.joinable()) {
		{
			std::lock_guard<std::mutex> g(lock);
			stop = true;
		}
		wake.notify_all();
		thread.join();
	}
	stop = false;
}

void decoding_source::stop_thread()
{
	halt_thread();
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
	if (thread_generation != generation) {
		free_frame(f);
		return;
	}
	/* without a time stamp, right after the one before */
	if (pts < 0)
		pts = queue.empty() ? 0 : queue.back().pts;
	if (start < 0)
		start = pts;
	queue.push_back({ pts, f });
}

/* The frame shown stays until one of the new place falls due. Finding the
   place can take a while in a large file, so the thread does it, and the
   emulation goes on meanwhile. */
void decoding_source::seek(double t)
{
	bool restart;
	{
		std::lock_guard<std::mutex> g(lock);
		seek_pts = (start >= 0 ? start : 0) + t;
		seek_t = t;
		generation++;
		for (auto &q : queue)
			free_frame(q.f);
		queue.clear();
		eof = false;
		restart = !running;
	}
	/* a thread that had come to the end of the file starts anew */
	if (restart) {
		halt_thread();
		start_thread();
	}
	wake.notify_all();
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
	/* sound from before the start, or from before where a seek went to,
	   is left out */
	if (pts >= 0 && start >= 0) {
		int pad = (int)((pts - start - audio_origin) * audio_rate);
		if (pad > 0 && pad < AUDIO_FRAMES / 2)
			audio_write(NULL, pad);
		else if (pad < 0)
			audio_skip = -pad;
	}
}

void decoding_source::audio_write(const uae_s16 *in, int n)
{
	if (thread_generation != generation)
		return;
	if (audio_skip > 0) {
		int k = n < audio_skip ? n : audio_skip;
		audio_skip -= k;
		n -= k;
		if (in)
			in += k * 2;
	}
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
		double pts = -1, t = 0;
		{
			std::unique_lock<std::mutex> g(lock);
			/* the sound is used up without a word to here, so look again
			   now and then */
			wake.wait_for(g, std::chrono::milliseconds(10), [this] { return stop || seek_pts >= 0 || want_more(); });
			if (stop) {
				running = false;
				return;
			}
			if (seek_pts >= 0) {
				pts = seek_pts;
				t = seek_t;
				seek_pts = -1;
			} else if (!want_more()) {
				continue;
			}
			thread_generation = generation;
		}
		if (pts >= 0) {
			auto began = std::chrono::steady_clock::now();
			seek_to(pts);
			audio_started = false;
			audio_origin = t;
			audio_skip = 0;
			audio_floor.store(audio_write_pos.load(std::memory_order_relaxed), std::memory_order_release);
			write_log(_T("HostPlay: seek to %.1f s found in %d ms\n"), t,
				(int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count());
			continue;
		}
		if (!decode_some()) {
			std::lock_guard<std::mutex> g(lock);
			/* the end, unless a seek came meanwhile */
			if (seek_pts >= 0)
				continue;
			eof = true;
			running = false;
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
	/* the sound from before the last seek goes */
	unsigned int floor = audio_floor.load(std::memory_order_acquire);
	if ((int)(floor - r) > 0)
		r = floor;
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
	void render(uae_u8 *dst, int w, int h, int stride, int format);

protected:
	bool decode_some();
	void free_frame(void *f) { AVFrame *a = (AVFrame *)f; av_frame_free(&a); }
	void seek_to(double pts);

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
	if (fmt->duration != AV_NOPTS_VALUE && fmt->duration > 0)
		duration = (double)fmt->duration / AV_TIME_BASE;
	snprintf(this->codec, sizeof this->codec, "%s", codec->name);
	write_log(_T("HostPlay: %s, %s %dx%d, %.1f s\n"), path, codec->name, width, height, duration);
	start_thread();
	return true;
}

void ffmpeg_source::seek_to(double pts)
{
	av_seek_frame(fmt, -1, (int64_t)(pts * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(dec);
	if (adec)
		avcodec_flush_buffers(adec);
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

void ffmpeg_source::render(uae_u8 *dst, int w, int h, int stride, int format)
{
	AVFrame *cur = (AVFrame *)current;
	if (!cur) {
		clear(dst, w, h, stride, format);
		return;
	}
	enum AVPixelFormat out = ffmpeg_format(format);
	sws = sws_getCachedContext(sws, cur->width, cur->height, (enum AVPixelFormat)cur->format,
		w, h, out, SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws)
		return;
	uint8_t *planes[4] = { dst, NULL, NULL, NULL };
	int strides[4] = { stride, 0, 0, 0 };
	sws_scale(sws, cur->data, cur->linesize, 0, cur->height, planes, strides);
}

#endif /* HOSTVIDEO_FFMPEG */

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

/* One picture, in memory: it falls due at once, and stays. */
class picture_source : public video_source {
public:
	picture_source() { dropped = 0; audio_rate = 0; duration = 0; codec[0] = 0; }
	bool take(double t) { if (taken) return false; taken = true; return true; }
	void render(uae_u8 *dst, int w, int h, int stride, int format);
	bool ended() { return false; }
	void seek(double t) { taken = false; }
	int audio_read(uae_s16 (*out)[2], int n) { return 0; }

protected:
	bool taken = false;
	int pw = 0, ph = 0;
	std::vector<uae_u8> rgba;
};

/* The picture scaled to w x h, bilinear, in a Picasso96 RGBFormat; what
   is see-through lies on black. */
void picture_source::render(uae_u8 *dst, int w, int h, int stride, int format)
{
	if (rgba.empty()) {
		clear(dst, w, h, stride, format);
		return;
	}
	int bpp = format_bpp(format);
	std::vector<int> xs(w), xf(w);
	for (int x = 0; x < w; x++) {
		int sx = (int)(((2 * x + 1) * (int64_t)pw * 128) / w) - 128;
		if (sx < 0)
			sx = 0;
		xs[x] = sx >> 8;
		xf[x] = sx & 255;
		if (xs[x] >= pw - 1) {
			xs[x] = pw - 1;
			xf[x] = 0;
		}
	}
	for (int y = 0; y < h; y++) {
		int sy = (int)(((2 * y + 1) * (int64_t)ph * 128) / h) - 128;
		if (sy < 0)
			sy = 0;
		int ys = sy >> 8, yf = sy & 255;
		if (ys >= ph - 1) {
			ys = ph - 1;
			yf = 0;
		}
		const uae_u8 *l0 = &rgba[(size_t)ys * pw * 4];
		const uae_u8 *l1 = yf ? l0 + (size_t)pw * 4 : l0;
		uae_u8 *p = dst + (size_t)y * stride;
		for (int x = 0; x < w; x++, p += bpp) {
			const uae_u8 *a = l0 + xs[x] * 4, *b = l1 + xs[x] * 4;
			int xw = xf[x], o = xw ? 4 : 0;
			int c[3];
			for (int k = 0; k < 3; k++) {
				int top = a[k] * (256 - xw) + a[k + o] * xw;
				int bot = b[k] * (256 - xw) + b[k + o] * xw;
				c[k] = (top * (256 - yf) + bot * yf) >> 16;
			}
			put_pixel(p, format, c[0], c[1], c[2]);
		}
	}
}

/* Pictures of the Amiga, which no decoder of the device knows: IFF ILBM with
   up to 8 planes, EHB, HAM6, HAM8, 24 and 32 planes, and the chunky PBM of
   Deluxe Paint. */

#define ILBM_MAX_FILE (64 * 1024 * 1024)
#define ILBM_MAX_PIXELS (64 * 1024 * 1024)
#define ILBM_CAMG_EHB 0x80
#define ILBM_CAMG_HAM 0x800

static inline uae_u32 ilbm_be32(const uae_u8 *p)
{
	return ((uae_u32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* Unpacks BODY into rows of rowbytes each; what is missing stays 0. */
static void ilbm_body(const uae_u8 *in, size_t inlen, bool packed, uae_u8 *out, size_t outlen)
{
	if (!packed) {
		memcpy(out, in, inlen < outlen ? inlen : outlen);
		return;
	}
	size_t i = 0, o = 0;
	while (i < inlen && o < outlen) {
		int n = (signed char)in[i++];
		if (n >= 0) {
			size_t c = (size_t)n + 1;
			if (c > inlen - i)
				c = inlen - i;
			if (c > outlen - o)
				c = outlen - o;
			memcpy(out + o, in + i, c);
			i += (size_t)n + 1;
			o += c;
		} else if (n != -128) {
			if (i >= inlen)
				break;
			size_t c = (size_t)(1 - n);
			if (c > outlen - o)
				c = outlen - o;
			memset(out + o, in[i++], c);
			o += c;
		}
	}
}

/* Decodes the file data[0..len) into w x h RGBA. The picture's shape, from
   the aspect of its pixels, goes to shown_w x shown_h. */
static bool ilbm_decode(const uae_u8 *data, size_t len, std::vector<uae_u8> &rgba,
	int *pw, int *ph, int *shown_w, int *shown_h, char *kind, size_t kindsize, int depth = 0)
{
	if (len < 12 || memcmp(data, "FORM", 4))
		return false;
	bool pbm = !memcmp(data + 8, "PBM ", 4);
	if (!pbm && memcmp(data + 8, "ILBM", 4))
		return false;

	const uae_u8 *bmhd = NULL, *cmap = NULL, *body = NULL;
	size_t ncolors = 0, bodylen = 0;
	uae_u32 camg = 0;
	bool have_camg = false;
	size_t at = 12;
	while (at + 8 <= len) {
		const uae_u8 *id = data + at;
		size_t size = ilbm_be32(id + 4);
		const uae_u8 *chunk = id + 8;
		size_t left = len - at - 8;
		if (size > left)
			size = left;		/* cut short: as much as there is */
		if (!memcmp(id, "BMHD", 4) && size >= 20)
			bmhd = chunk;
		else if (!memcmp(id, "CMAP", 4)) {
			cmap = chunk;
			ncolors = size / 3;
		} else if (!memcmp(id, "CAMG", 4) && size >= 4) {
			camg = ilbm_be32(chunk);
			have_camg = true;
		} else if (!memcmp(id, "BODY", 4)) {
			body = chunk;
			bodylen = size;
		} else if (!memcmp(id, "FORM", 4) && !bmhd && size >= 12 && depth < 2) {
			/* a picture wrapped in another, as MUI's patterns are */
			return ilbm_decode(id, size + 8, rgba, pw, ph, shown_w, shown_h, kind, kindsize, depth + 1);
		}
		at += 8 + size + (size & 1);
	}
	if (!bmhd || !body)
		return false;

	int w = (bmhd[0] << 8) | bmhd[1], h = (bmhd[2] << 8) | bmhd[3];
	int planes = bmhd[8], masking = bmhd[9], compression = bmhd[10];
	int xaspect = bmhd[14], yaspect = bmhd[15];
	if (w <= 0 || h <= 0 || (int64_t)w * h > ILBM_MAX_PIXELS || compression > 1)
		return false;
	if (pbm ? planes != 8 && planes != 24 : !((planes >= 1 && planes <= 8) || planes == 24 || planes == 32))
		return false;

	/* the mask, an extra plane, is there only to be passed over */
	size_t planerow = (size_t)((w + 15) >> 4) * 2;
	size_t rowbytes = pbm ? (size_t)(((int64_t)w * planes / 8 + 1) & ~1)
		: planerow * (planes + (masking == 1 ? 1 : 0));
	std::vector<uae_u8> rows;
	rows.resize(rowbytes * h);
	ilbm_body(body, bodylen, compression == 1, rows.data(), rows.size());

	/* the colours; files from before colours had 8 bits hold 4 in the top */
	uae_u8 pal[256][3];
	memset(pal, 0, sizeof pal);
	int count = ncolors > 256 ? 256 : (int)ncolors;
	bool fours = count > 0;
	for (int i = 0; i < count * 3; i++)
		fours = fours && !(cmap[i] & 0x0f);
	for (int i = 0; i < count; i++) {
		for (int k = 0; k < 3; k++) {
			int v = cmap[i * 3 + k];
			pal[i][k] = fours ? v | (v >> 4) : v;
		}
	}
	if (!count && planes <= 8) {
		for (int i = 0; i < (1 << planes); i++)
			pal[i][0] = pal[i][1] = pal[i][2] = i * 255 / ((1 << planes) - 1);
	}

	bool ham = false, ehb = false;
	if (!pbm && planes <= 8) {
		/* a value with a monitor's number above the low 16 bits is an RTG
		   ModeID, whose bits mean nothing of HAM */
		if (have_camg && (camg >> 24) == 0) {
			ham = (camg & ILBM_CAMG_HAM) && (planes == 6 || planes == 8 || planes == 5 || planes == 7);
			ehb = (camg & ILBM_CAMG_EHB) && planes == 6;
		} else if (have_camg) {
		} else if (planes == 6) {
			/* an old file with no CAMG: the palette tells */
			ham = count == 16;
			ehb = count == 32;
		} else if (planes == 8) {
			ham = count == 64;
		}
	}
	if (ehb) {
		for (int i = 0; i < 32; i++) {
			for (int k = 0; k < 3; k++)
				pal[32 + i][k] = pal[i][k] >> 1;
		}
	}

	rgba.resize((size_t)w * h * 4);
	int hambits = planes > 6 ? 6 : 4;	/* the bits of a modified colour */
	for (int y = 0; y < h; y++) {
		const uae_u8 *row = rows.data() + rowbytes * y;
		uae_u8 *out = rgba.data() + (size_t)w * y * 4;
		int r = pal[0][0], g = pal[0][1], b = pal[0][2];
		for (int x = 0; x < w; x++, out += 4) {
			uae_u32 v = 0;
			if (pbm) {
				if (planes == 8) {
					v = row[x];
				} else {
					const uae_u8 *c = row + x * 3;
					v = c[0] | (c[1] << 8) | (c[2] << 16);
				}
			} else {
				int byte = x >> 3, bit = 7 - (x & 7);
				for (int p = 0; p < planes; p++)
					v |= (uae_u32)((row[planerow * p + byte] >> bit) & 1) << p;
			}
			if (planes > 8) {
				r = v & 255;
				g = (v >> 8) & 255;
				b = (v >> 16) & 255;
			} else if (ham) {
				int low = v & ((1 << hambits) - 1), mode = v >> hambits;
				int full = hambits == 4 ? low * 17 : (low << 2) | (low >> 4);
				switch (mode) {
				case 1: b = full; break;
				case 2: r = full; break;
				case 3: g = full; break;
				default:
					r = pal[low][0];
					g = pal[low][1];
					b = pal[low][2];
					break;
				}
			} else {
				r = pal[v][0];
				g = pal[v][1];
				b = pal[v][2];
			}
			out[0] = r;
			out[1] = g;
			out[2] = b;
			out[3] = 255;
		}
	}

	*pw = *shown_w = w;
	*ph = *shown_h = h;
	/* pixels that are not square come out taller or wider, never smaller;
	   an aspect of nonsense is left alone */
	if (xaspect && yaspect && xaspect * 4 >= yaspect && yaspect * 4 >= xaspect) {
		if (yaspect > xaspect)
			*shown_h = (int)((int64_t)h * yaspect / xaspect);
		else if (xaspect > yaspect)
			*shown_w = (int)((int64_t)w * xaspect / yaspect);
		if (*shown_w > 0xffff || *shown_h > 0xffff) {
			*shown_w = w;
			*shown_h = h;
		}
	}
	snprintf(kind, kindsize, "%s", pbm ? "PBM" : ham ? (planes > 6 ? "HAM8" : "HAM6")
		: ehb ? "EHB" : "ILBM");
	return true;
}

class ilbm_source : public picture_source {
public:
	bool open(const char *path);
};

bool ilbm_source::open(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	uae_u8 head[12];
	bool ok = fread(head, 1, 12, f) == 12 && !memcmp(head, "FORM", 4) &&
		(!memcmp(head + 8, "ILBM", 4) || !memcmp(head + 8, "PBM ", 4));
	std::vector<uae_u8> file;
	if (ok) {
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		ok = size >= 12 && size <= ILBM_MAX_FILE;
		if (ok) {
			file.resize(size);
			fseek(f, 0, SEEK_SET);
			ok = fread(file.data(), 1, file.size(), f) == file.size();
		}
	}
	fclose(f);
	if (ok)
		ok = ilbm_decode(file.data(), file.size(), rgba, &pw, &ph, &width, &height, codec, sizeof codec);
	if (ok)
		write_log(_T("HostPlay: %s, %s %dx%d, shown at %dx%d\n"), path, codec, pw, ph, width, height);
	return ok;
}

#ifdef HOSTVIDEO_MEDIACODEC

/* One pixel in the screen's format, as it lies in the Amiga's memory: the
   PC formats are little endian words. */
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
static void yuv_render(const yuv_frame *f, uae_u8 *dst, int w, int h, int stride, int format, bool bt709, bool full_range)
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
		uae_u8 *p = dst + (size_t)y * stride;
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

/* A second reader of the file, which goes through it to the end in the
   background. Android's reader of a fragmented MP4 - what downloaders
   often save - finds a place by going through the fragments up to it, the
   first time: seconds for a long video. Once this one went through them
   all, it takes over at the next seek, and seeks are quick from then on.
   Shared with its thread, which outlives a player that closes meanwhile. */
struct scout_reader {
	std::mutex lock;
	bool ready = false, abandoned = false;
	int fd = -1;
	AMediaExtractor *ex = NULL;
};

static void scout_run(std::shared_ptr<scout_reader> sc, std::string path, int vtrack, int atrack, int64_t end_us)
{
	auto began = std::chrono::steady_clock::now();
	int fd = ::open(path.c_str(), O_RDONLY);
	struct stat st;
	AMediaExtractor *ex = NULL;
	if (fd >= 0 && fstat(fd, &st) == 0) {
		ex = AMediaExtractor_new();
		if (ex && AMediaExtractor_setDataSourceFd(ex, fd, 0, st.st_size) == AMEDIA_OK) {
			AMediaExtractor_selectTrack(ex, vtrack);
			if (atrack >= 0)
				AMediaExtractor_selectTrack(ex, atrack);
			AMediaExtractor_seekTo(ex, end_us, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
		} else if (ex) {
			AMediaExtractor_delete(ex);
			ex = NULL;
		}
	}
	std::lock_guard<std::mutex> g(sc->lock);
	if (ex && !sc->abandoned) {
		sc->ex = ex;
		sc->fd = fd;
		sc->ready = true;
		write_log(_T("HostPlay: went through the file in %d ms\n"),
			(int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count());
		return;
	}
	if (ex)
		AMediaExtractor_delete(ex);
	if (fd >= 0)
		close(fd);
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
	void render(uae_u8 *dst, int w, int h, int stride, int format);

protected:
	bool decode_some();
	void free_frame(void *f) { delete (yuv_frame *)f; }
	void seek_to(double pts);

private:
	bool feed();
	bool drain_video();
	bool drain_audio();
	void video_format();

	int fd = -1;
	AMediaExtractor *ex = NULL;
	std::shared_ptr<scout_reader> scout;
	AMediaCodec *vcodec = NULL, *acodec = NULL;
	int vtrack = -1, atrack = -1;
	bool input_done = false, veos_sent = false, aeos_sent = false;
	bool video_done = false, audio_done = false;

	/* the decoder's pictures; with the flexible colour format, whether they
	   are planar, as from Google's software decoders, or NV12 */
	int pic_w = 0, pic_h = 0, stride = 0, slice_height = 0, color = 0;
	bool flexible_planar = false;
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
		int64_t us = 0;
		if (AMediaFormat_getInt64(vfmt, AMEDIAFORMAT_KEY_DURATION, &us) && us > 0)
			duration = us / 1000000.0;
		snprintf(codec, sizeof codec, "%s", vmime + 6);
		vcodec = AMediaCodec_createDecoderByType(vmime);
		/* pictures in memory, in one of the YUV 4:2:0 layouts */
		AMediaFormat_setInt32(vfmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_YUV420_FLEXIBLE);
		if (vcodec && AMediaCodec_configure(vcodec, vfmt, NULL, NULL, 0) == AMEDIA_OK &&
				AMediaCodec_start(vcodec) == AMEDIA_OK) {
			AMediaExtractor_selectTrack(ex, vtrack);
			/* The decoder's name, from Android 9 on: the hardware ones put the
			   flexible format out as NV12, Google's software ones planar. */
			typedef media_status_t (*getname_t)(AMediaCodec *, char **);
			typedef void (*releasename_t)(AMediaCodec *, char *);
			getname_t getname = (getname_t)dlsym(RTLD_DEFAULT, "AMediaCodec_getName");
			releasename_t releasename = (releasename_t)dlsym(RTLD_DEFAULT, "AMediaCodec_releaseName");
			char *name = NULL;
			if (getname && getname(vcodec, &name) == AMEDIA_OK && name) {
				flexible_planar = !strncmp(name, "c2.android.", 11) || !strncmp(name, "OMX.google.", 11);
				write_log(_T("HostPlay: %s, %s %dx%d, decoder %s\n"), path, vmime, width, height, name);
				if (releasename)
					releasename(vcodec, name);
			} else {
				write_log(_T("HostPlay: %s, %s %dx%d\n"), path, vmime, width, height);
			}
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
	if (duration > 0) {
		scout = std::make_shared<scout_reader>();
		std::thread(scout_run, scout, std::string(path), vtrack, acodec ? atrack : -1, (int64_t)(duration * 1000000)).detach();
	}
	return true;
}

mediacodec_source::~mediacodec_source()
{
	stop_thread();
	if (scout) {
		std::lock_guard<std::mutex> g(scout->lock);
		scout->abandoned = true;
		if (scout->ex)
			AMediaExtractor_delete(scout->ex);
		if (scout->fd >= 0)
			close(scout->fd);
		scout->ex = NULL;
		scout->fd = -1;
	}
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

void mediacodec_source::seek_to(double pts)
{
	if (scout) {
		std::lock_guard<std::mutex> g(scout->lock);
		if (scout->ready) {
			/* the reader that went through the file takes over */
			AMediaExtractor_delete(ex);
			close(fd);
			ex = scout->ex;
			fd = scout->fd;
			scout->ex = NULL;
			scout->fd = -1;
			scout->ready = false;
		}
	}
	AMediaExtractor_seekTo(ex, (int64_t)(pts * 1000000), AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
	AMediaCodec_flush(vcodec);
	if (acodec)
		AMediaCodec_flush(acodec);
	input_done = veos_sent = aeos_sent = false;
	video_done = audio_done = false;
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
		bool planar = color == COLOR_YUV420_PLANAR || (color == COLOR_YUV420_FLEXIBLE && flexible_planar);
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

void mediacodec_source::render(uae_u8 *dst, int w, int h, int stride, int format)
{
	yuv_frame *f = (yuv_frame *)current;
	if (!f || f->w < 2 || f->h < 2) {
		clear(dst, w, h, stride, format);
		return;
	}
	yuv_render(f, dst, w, h, stride, format, bt709, full_range);
}

/* A still picture, for HostView: Android's own image decoder (AImageDecoder,
   Android 11 on) - JPEG, PNG, WebP, GIF, BMP, and HEIF and AVIF through
   the device's video decoders. Looked up at run time, as the app starts on
   older Androids too. */
struct AImageDecoder;
struct AImageDecoderHeaderInfo;
#define ANDROID_BITMAP_FORMAT_RGBA_8888 1
#define ANDROID_IMAGE_DECODER_SUCCESS 0

static struct {
	bool tried, found;
	int (*createFromFd)(int, AImageDecoder **);
	void (*del)(AImageDecoder *);
	const AImageDecoderHeaderInfo *(*getHeaderInfo)(const AImageDecoder *);
	int32_t (*getWidth)(const AImageDecoderHeaderInfo *);
	int32_t (*getHeight)(const AImageDecoderHeaderInfo *);
	const char *(*getMimeType)(const AImageDecoderHeaderInfo *);
	int (*setFormat)(AImageDecoder *, int32_t);
	int (*setTargetSize)(AImageDecoder *, int32_t, int32_t);
	size_t (*getMinimumStride)(AImageDecoder *);
	int (*decodeImage)(AImageDecoder *, void *, size_t, size_t);
} imagedecoder;

static bool find_imagedecoder(void)
{
	if (imagedecoder.tried)
		return imagedecoder.found;
	imagedecoder.tried = true;
	void *lib = dlopen("libjnigraphics.so", RTLD_NOW);
	if (!lib)
		return false;
#define FIND(f, name) *(void **)&imagedecoder.f = dlsym(lib, name)
	FIND(createFromFd, "AImageDecoder_createFromFd");
	FIND(del, "AImageDecoder_delete");
	FIND(getHeaderInfo, "AImageDecoder_getHeaderInfo");
	FIND(getWidth, "AImageDecoderHeaderInfo_getWidth");
	FIND(getHeight, "AImageDecoderHeaderInfo_getHeight");
	FIND(getMimeType, "AImageDecoderHeaderInfo_getMimeType");
	FIND(setFormat, "AImageDecoder_setAndroidBitmapFormat");
	FIND(setTargetSize, "AImageDecoder_setTargetSize");
	FIND(getMinimumStride, "AImageDecoder_getMinimumStride");
	FIND(decodeImage, "AImageDecoder_decodeImage");
#undef FIND
	imagedecoder.found = imagedecoder.createFromFd && imagedecoder.del && imagedecoder.getHeaderInfo &&
		imagedecoder.getWidth && imagedecoder.getHeight && imagedecoder.getMimeType &&
		imagedecoder.setFormat && imagedecoder.setTargetSize && imagedecoder.getMinimumStride &&
		imagedecoder.decodeImage;
	if (!imagedecoder.found)
		write_log(_T("HostPlay: no AImageDecoder, which Android 11 brought\n"));
	return imagedecoder.found;
}

/* Pictures larger than this, on their longer side, are decoded smaller:
   still more than a screen, without hundreds of MB for a photo. */
#define IMAGE_MAX_SIDE 2560

class image_source : public picture_source {
public:
	bool open(const char *path);
};

bool image_source::open(const char *path)
{
	if (!find_imagedecoder())
		return false;
	int fd = ::open(path, O_RDONLY);
	if (fd < 0)
		return false;
	AImageDecoder *dec = NULL;
	bool ok = imagedecoder.createFromFd(fd, &dec) == ANDROID_IMAGE_DECODER_SUCCESS && dec;
	if (ok) {
		const AImageDecoderHeaderInfo *info = imagedecoder.getHeaderInfo(dec);
		width = imagedecoder.getWidth(info);
		height = imagedecoder.getHeight(info);
		const char *mime = imagedecoder.getMimeType(info);
		snprintf(codec, sizeof codec, "%s", mime && !strncmp(mime, "image/", 6) ? mime + 6 : (mime ? mime : "?"));
		pw = width;
		ph = height;
		if (pw > IMAGE_MAX_SIDE || ph > IMAGE_MAX_SIDE) {
			if (pw >= ph) {
				ph = (int)((int64_t)ph * IMAGE_MAX_SIDE / pw);
				pw = IMAGE_MAX_SIDE;
			} else {
				pw = (int)((int64_t)pw * IMAGE_MAX_SIDE / ph);
				ph = IMAGE_MAX_SIDE;
			}
			if (pw < 1)
				pw = 1;
			if (ph < 1)
				ph = 1;
		}
		ok = width > 0 && height > 0 && width <= 0xffff && height <= 0xffff &&
			imagedecoder.setFormat(dec, ANDROID_BITMAP_FORMAT_RGBA_8888) == ANDROID_IMAGE_DECODER_SUCCESS &&
			((pw == width && ph == height) || imagedecoder.setTargetSize(dec, pw, ph) == ANDROID_IMAGE_DECODER_SUCCESS);
	}
	if (ok) {
		size_t stride = imagedecoder.getMinimumStride(dec);
		std::vector<uae_u8> buf(stride * ph);
		ok = imagedecoder.decodeImage(dec, buf.data(), stride, buf.size()) == ANDROID_IMAGE_DECODER_SUCCESS;
		if (ok) {
			rgba.resize((size_t)pw * ph * 4);
			for (int y = 0; y < ph; y++)
				memcpy(&rgba[(size_t)y * pw * 4], &buf[(size_t)y * stride], (size_t)pw * 4);
			write_log(_T("HostPlay: %s, %s %dx%d, decoded at %dx%d\n"), path, codec, width, height, pw, ph);
		}
	}
	if (dec)
		imagedecoder.del(dec);
	close(fd);
	return ok;
}

#endif /* HOSTVIDEO_MEDIACODEC */

/* A source for the file, or NULL. */
static video_source *open_source(const char *path)
{
	ilbm_source *pic = new ilbm_source();
	if (pic->open(path))
		return pic;
	delete pic;
#if defined(HOSTVIDEO_FFMPEG)
	ffmpeg_source *s = new ffmpeg_source();
#elif defined(HOSTVIDEO_MEDIACODEC)
	mediacodec_source *s = new mediacodec_source();
#endif
#if defined(HOSTVIDEO_FFMPEG) || defined(HOSTVIDEO_MEDIACODEC)
	if (s->open(path))
		return s;
	delete s;
#endif
#if defined(HOSTVIDEO_MEDIACODEC)
	/* FFmpeg takes pictures as videos of a frame; MediaCodec does not */
	image_source *i = new image_source();
	if (i->open(path))
		return i;
	delete i;
#endif
	write_log(_T("HostPlay: %s does not play\n"), path);
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
	/* a new frame or the end the tool has not seen yet, and it was told;
	   the end once told is not told again */
	bool ready, ended, signalled, end_told;
	/* a seek waits for its first frame: the clock stands until then */
	bool seeking;
	bool fullscreen; /* on a screen of its own */
	std::chrono::steady_clock::time_point seek_began;

	int stat_shown;
	int stat_time;

	std::vector<uae_u8> pixels;

	/* a copy of the file, deleted with the player */
	char copy[MAX_DPATH];

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
	if (!v->paused && !v->seeking) {
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

/* The frame in the shape of the video, in the middle of the window, with
   black bars where the window's shape differs. */
static void render(struct hostvideo *v)
{
	if (!v->buffer || !v->bpp)
		return;
	int w = v->target_w, h = v->target_h;
	int vw = v->source->width, vh = v->source->height;
	int fw = w, fh = (int)((int64_t)w * vh / vw);
	if (fh > h) {
		fh = h;
		fw = (int)((int64_t)h * vw / vh);
	}
	if (v->fullscreen) {
		/* a screen of its own may be shown wider or narrower than its
		   pixels say: the shape has to come out right where it is seen */
		int sw, sh;
		rtg_shown_shape(w, h, &sw, &sh);
		double scale = (double)sw / vw;
		if ((double)sh / vh < scale)
			scale = (double)sh / vh;
		fw = (int)(vw * scale * w / sw + 0.5);
		fh = (int)(vh * scale * h / sh + 0.5);
		if (fw > w)
			fw = w;
		if (fh > h)
			fh = h;
	}
	if (fw < 1)
		fw = 1;
	if (fh < 1)
		fh = 1;
	int stride = w * v->bpp;
	if (fw != w || fh != h)
		clear(v->pixels.data(), w, h, stride, v->format);
	uae_u8 *at = v->pixels.data() + (size_t)((h - fh) / 2) * stride + (size_t)((w - fw) / 2) * v->bpp;
	v->source->render(at, fw, fh, stride, v->format);
	copy_to_amiga(v->buffer, v->pixels.data(), (uae_u32)v->pixels.size());
}

/* Runs the video's clock up to the current emulated time and takes the
   frame due by then. */
static void catchup(struct hostvideo *v)
{
	uae_u32 now = get_cycles();
	uae_u32 delta = now - v->last_cycles;
	v->last_cycles = now;
	if (v->ended)
		return;
	double second = resampler_clock() * CYCLE_UNIT;
	/* after a pause of the emulator, not a run of frames at once */
	if (delta > second / 4)
		delta = (uae_u32)(second / 4);
	/* paused, the clock stands, but a seek still shows its frame */
	if (!v->paused && !v->seeking)
		v->played += delta;
	double t = v->played / second;
	if (v->source->take(t)) {
		v->ready = true;
		if (v->seeking) {
			v->seeking = false;
			write_log(_T("HostPlay: seek to %.1f s played in %d ms\n"), t,
				(int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - v->seek_began).count());
		}
	} else if (v->source->ended()) {
		v->ended = true;
		v->seeking = false;
	}
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
		if ((v->ready || (v->ended && !v->end_told)) && !v->signalled && v->buffer) {
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
	if (v->copy[0]) {
		unlink(v->copy);
		v->copy[0] = 0;
	}
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

/* A file of a folder drive by its Lock, or with no Lock the copy uaelib
   made of one the host does not reach */
static uae_u32 player_open(TrapContext *ctx, uaecptr task, uae_u32 sigmask, uae_u32 lock)
{
	int i;
	for (i = 0; i < HOSTVIDEO_PLAYERS; i++) {
		if (!players[i].used)
			break;
	}
	if (i == HOSTVIDEO_PLAYERS) {
		if (!lock) {
			char copy[MAX_DPATH];
			if (emulib_take_copy(copy, sizeof copy))
				unlink(copy);
		}
		return HV_OPEN_BUSY;
	}
	TCHAR path[MAX_DPATH];
	bool copy = !lock;
	if (copy) {
		if (!emulib_take_copy(path, sizeof path))
			return HV_OPEN_CANNOT;
	} else if (get_native_path(ctx, lock, path)) {
		return HV_OPEN_NOPATH;
	}
	video_source *source = open_source(path);
	if (!source) {
		if (copy)
			unlink(path);
		return HV_OPEN_CANNOT;
	}
	struct hostvideo *v = &players[i];
	if (copy)
		snprintf(v->copy, sizeof v->copy, "%s", path);
	else
		v->copy[0] = 0;
	v->used = true;
	v->task = task;
	v->sigmask = sigmask;
	v->source = source;
	v->buffer = 0;
	v->bpp = 0;
	v->paused = false;
	v->last_cycles = get_cycles();
	v->played = 0;
	v->ready = v->ended = v->signalled = v->end_told = false;
	v->seeking = false;
	v->fullscreen = false;
	v->stat_shown = v->stat_time = 0;
	v->apos = v->alen = v->missed = 0;
	v->quiet = RS_HIST;
	if (source->audio_rate > 0)
		resampler_start(&v->rs, source->audio_rate);
	update_playing();
	return i + 1;
}

/* The n-th size of RTG mode for showing a picture of pw x ph on a screen of
   its own. Only sizes of the Workbench's proportions, which it has itself
   among them: they are shown the way it is. First those that hold the whole
   picture, the smallest first, then the others, the largest first. A mode
   with more pixels than the device shows gives nothing but a shrunk screen,
   so such a size counts as holding the picture. */
static uae_u32 pick_mode(int pw, int ph, int n, int wbw, int wbh)
{
	int sw, sh;
	if (wbw <= 0 || wbh <= 0 || !DisplayModes)
		return 0;
	rtg_shown_shape(wbw, wbh, &sw, &sh);
	/* the size of the Workbench's shape that holds the picture */
	double need = (double)pw / wbw;
	if ((double)ph / wbh > need)
		need = (double)ph / wbh;
	if (need > (double)sw / wbw && need > (double)sh / wbh)
		need = (double)sw / wbw > (double)sh / wbh ? (double)sw / wbw : (double)sh / wbh;
	int needw = (int)(wbw * need);
	struct { int w, h; } modes[MAX_PICASSO_MODES];
	int count = 0;
	for (int i = 0; DisplayModes[i].depth >= 0; i++) {
		int w = DisplayModes[i].res.width, h = DisplayModes[i].res.height;
		/* within 2 per cent; a width comes rounded to 16 */
		int64_t a = (int64_t)w * wbh, b = (int64_t)h * wbw;
		if (llabs(a - b) * 50 > b)
			continue;
		bool have = false;
		for (int j = 0; j < count; j++)
			have = have || (modes[j].w == w && modes[j].h == h);
		if (!have) {
			modes[count].w = w;
			modes[count].h = h;
			count++;
		}
	}
	for (int i = 0; i < count; i++) {
		for (int j = i + 1; j < count; j++) {
			bool ibig = modes[i].w >= needw, jbig = modes[j].w >= needw;
			bool swap = ibig != jbig ? jbig
				: ibig ? modes[j].w < modes[i].w : modes[j].w > modes[i].w;
			if (swap) {
				int t = modes[i].w; modes[i].w = modes[j].w; modes[j].w = t;
				t = modes[i].h; modes[i].h = modes[j].h; modes[j].h = t;
			}
		}
	}
	if (n < 0 || n >= count)
		return 0;
	return (modes[n].w << 16) | modes[n].h;
}

uae_u32 hostvideo_call(TrapContext *ctx, uae_u32 cmd, uae_u32 arg1, uae_u32 arg2, uae_u32 arg3, uae_u32 arg4)
{
	if (cmd == HV_CMD_VERSION)
		return HV_API_VERSION;
	if (cmd == HV_CMD_OPEN)
		return arg3 ? player_open(ctx, arg1, arg2, arg3) : HV_OPEN_NOPATH;
	if (cmd == HV_CMD_OPENCOPY)
		return player_open(ctx, arg1, arg2, 0);

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
	case HV_CMD_MODE:
		return pick_mode(v->source->width, v->source->height, (int)arg2, arg3 >> 16, arg3 & 0xffff);
	case HV_CMD_FULLSCREEN:
		v->fullscreen = arg2 != 0;
		return 1;
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
		if (v->ended) {
			v->end_told = true;
			return HV_FRAME_END;
		}
		return HV_FRAME_NONE;
	case HV_CMD_PAUSE:
		v->paused = !v->paused;
		return v->paused ? 1 : 0;
	case HV_CMD_TIME: {
		double t = v->played / (resampler_clock() * CYCLE_UNIT);
		if (v->source->duration > 0 && t > v->source->duration)
			t = v->source->duration;
		return (uae_u32)(t * 1000);
	}
	case HV_CMD_DURATION:
		return (uae_u32)(v->source->duration * 1000);
	case HV_CMD_SEEK: {
		double t = arg2 / 1000.0;
		if (v->source->duration > 0 && t > v->source->duration)
			t = v->source->duration;
		v->source->seek(t);
		v->played = (uae_s64)(t * resampler_clock() * CYCLE_UNIT);
		v->ready = v->ended = v->signalled = v->end_told = false;
		v->seeking = true;
		v->seek_began = std::chrono::steady_clock::now();
		v->stat_time = (int)t;
		v->apos = v->alen = v->missed = 0;
		if (v->source->audio_rate > 0)
			resampler_clear(&v->rs);
		v->quiet = RS_HIST;
		update_playing();
		return 1;
	}
	case HV_CMD_DESCRIBE: {
		char text[64];
		snprintf(text, sizeof text, "(%dx%d) (%s)", v->source->width, v->source->height, v->source->codec);
		if (!arg2 || !arg3)
			return 0;
		uae_u32 n = (uae_u32)strlen(text) + 1;
		if (n > arg3)
			n = arg3;
		for (uae_u32 i = 0; i < n; i++)
			put_byte(arg2 + i, i == n - 1 ? 0 : text[i]);
		return 1;
	}
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
