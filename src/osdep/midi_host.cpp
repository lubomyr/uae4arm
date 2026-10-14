/*
 * The host's MIDI, see midi_host.h.
 *
 * On the Linux PC build this is an ALSA sequencer client, "uae4arm", with one
 * port both ways. Its output is connected to the first synthesizer found, such
 * as FluidSynth; anything else can be connected with aconnect. On Android
 * it is the system's MIDI through the SDL port: a USB MIDI device, or an app
 * with a MIDI synthesizer.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "midi_host.h"

#ifdef LINUX_HOST

#include <alsa/asoundlib.h>

static snd_seq_t *seq;
static int port = -1;
static snd_midi_event_t *encoder, *decoder;

/* Bytes received, waiting for the Amiga */
static uae_u8 in_buf[4096];
static int in_read, in_write;

static void connect_to_synth(void)
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;
	int self = snd_seq_client_id(seq);

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		int client = snd_seq_client_info_get_client(cinfo);
		if (client == self)
			continue;
		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
			unsigned int caps = snd_seq_port_info_get_capability(pinfo);
			unsigned int type = snd_seq_port_info_get_type(pinfo);
			if ((caps & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
				continue;
			if (!(type & SND_SEQ_PORT_TYPE_SYNTHESIZER))
				continue;
			if (snd_seq_connect_to(seq, port, client, snd_seq_port_info_get_port(pinfo)) >= 0) {
				write_log(_T("MIDI: sending to %s (%d:%d)\n"), snd_seq_client_info_get_name(cinfo),
					client, snd_seq_port_info_get_port(pinfo));
				return;
			}
		}
	}
	write_log(_T("MIDI: no synthesizer found, connect uae4arm %d:%d with aconnect\n"), self, port);
}

bool midi_host_open(void)
{
	if (seq)
		return true;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
		write_log(_T("MIDI: cannot open the ALSA sequencer\n"));
		seq = NULL;
		return false;
	}
	snd_seq_set_client_name(seq, "uae4arm");
	port = snd_seq_create_simple_port(seq, "Amiga serial MIDI",
		SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	if (port < 0 || snd_midi_event_new(1024, &encoder) < 0 || snd_midi_event_new(1024, &decoder) < 0) {
		midi_host_close();
		return false;
	}
	// The Amiga parses running status itself; give it every status byte
	snd_midi_event_no_status(decoder, 1);
	in_read = in_write = 0;
	connect_to_synth();
	return true;
}

void midi_host_close(void)
{
	if (encoder)
		snd_midi_event_free(encoder);
	if (decoder)
		snd_midi_event_free(decoder);
	encoder = decoder = NULL;
	if (seq)
		snd_seq_close(seq);
	seq = NULL;
	port = -1;
}

bool midi_host_is_open(void)
{
	return seq != NULL;
}

void midi_host_device(bool input, TCHAR *name, int size)
{
	snd_seq_query_subscribe_t *subs;
	snd_seq_client_info_t *cinfo;
	snd_seq_addr_t addr;
	int len = 0;

	name[0] = 0;
	if (!seq)
		return;
	snd_seq_query_subscribe_alloca(&subs);
	snd_seq_client_info_alloca(&cinfo);
	addr.client = snd_seq_client_id(seq);
	addr.port = port;
	snd_seq_query_subscribe_set_root(subs, &addr);
	// Those reading from the port get what the Amiga sends
	snd_seq_query_subscribe_set_type(subs, input ? SND_SEQ_QUERY_SUBS_WRITE : SND_SEQ_QUERY_SUBS_READ);
	snd_seq_query_subscribe_set_index(subs, 0);
	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
		const snd_seq_addr_t *other = snd_seq_query_subscribe_get_addr(subs);
		if (snd_seq_get_any_client_info(seq, other->client, cinfo) >= 0)
			len += snprintf(name + len, size - len, "%s%s", len ? ", " : "", snd_seq_client_info_get_name(cinfo));
		if (len >= size - 1)
			break;
		snd_seq_query_subscribe_set_index(subs, snd_seq_query_subscribe_get_index(subs) + 1);
	}
}

void midi_host_out(uae_u8 b)
{
	snd_seq_event_t ev;

	if (!seq)
		return;
	snd_seq_ev_clear(&ev);
	if (snd_midi_event_encode_byte(encoder, b, &ev) != 1)
		return; // not a whole message yet
	snd_seq_ev_set_source(&ev, port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);
	snd_seq_event_output_direct(seq, &ev);
}

bool midi_host_in(uae_u8 *b)
{
	if (!seq)
		return false;
	while (in_read == in_write && snd_seq_event_input_pending(seq, 1) > 0) {
		snd_seq_event_t *ev;
		uae_u8 bytes[1024];
		if (snd_seq_event_input(seq, &ev) < 0)
			break;
		long n = snd_midi_event_decode(decoder, bytes, sizeof bytes, ev);
		for (long i = 0; i < n; i++) {
			int next = (in_write + 1) % (int)sizeof in_buf;
			if (next == in_read)
				break; // full; the Amiga is not reading
			in_buf[in_write] = bytes[i];
			in_write = next;
		}
	}
	if (in_read == in_write)
		return false;
	*b = in_buf[in_read];
	in_read = (in_read + 1) % (int)sizeof in_buf;
	return true;
}

#elif defined(ANDROIDSDL)

#include <SDL_android.h>

static bool midi_open;

/* The message being put together: Android synthesizer apps want whole
   messages, the Amiga sends a byte at a time and may use running status */
static uae_u8 msg[256];
static int msg_len, msg_need;
static uae_u8 running_status;
static bool in_sysex;

/* Length of a message with this status byte, 0 if it has no fixed length */
static int message_length(uae_u8 status)
{
	if (status < 0xc0 || (status >= 0xe0 && status < 0xf0))
		return 3;
	if (status < 0xe0)
		return 2;
	switch (status) {
		case 0xf1: case 0xf3:
			return 2;
		case 0xf2:
			return 3;
		case 0xf0:
			return 0;
		default:
			return 1;
	}
}

bool midi_host_open(void)
{
	if (midi_open)
		return true;
	if (!SDL_ANDROID_MidiOpen()) {
		write_log(_T("MIDI: the system has no MIDI\n"));
		return false;
	}
	msg_len = 0;
	running_status = 0;
	in_sysex = false;
	midi_open = true;
	return true;
}

void midi_host_close(void)
{
	if (midi_open)
		SDL_ANDROID_MidiClose();
	midi_open = false;
}

bool midi_host_is_open(void)
{
	return midi_open;
}

void midi_host_device(bool input, TCHAR *name, int size)
{
	name[0] = 0;
	if (midi_open)
		SDL_ANDROID_MidiDeviceName(input ? 1 : 0, name, size);
}

void midi_host_out(uae_u8 b)
{
	if (!midi_open)
		return;
	if (b >= 0xf8) {
		// Real time, can come between the bytes of any message
		SDL_ANDROID_MidiSend(&b, 1);
		return;
	}
	if (b & 0x80) {
		bool was_sysex = in_sysex;
		in_sysex = false;
		if (b == 0xf7) {
			if (was_sysex) {
				msg[msg_len++] = b;
				SDL_ANDROID_MidiSend(msg, msg_len);
			}
			msg_len = 0;
			return;
		}
		if (was_sysex && msg_len)
			SDL_ANDROID_MidiSend(msg, msg_len); // cut short by another status
		msg[0] = b;
		msg_len = 1;
		msg_need = message_length(b);
		running_status = b < 0xf0 ? b : 0;
		if (b == 0xf0)
			in_sysex = true;
		else if (msg_need == 1) {
			SDL_ANDROID_MidiSend(msg, 1);
			msg_len = 0;
		}
		return;
	}
	if (in_sysex) {
		msg[msg_len++] = b;
		if (msg_len == (int)sizeof msg) {
			SDL_ANDROID_MidiSend(msg, msg_len);
			msg_len = 0;
		}
		return;
	}
	if (msg_len == 0) {
		if (!running_status)
			return; // data without a status, nothing to give it to
		msg[0] = running_status;
		msg_len = 1;
		msg_need = message_length(running_status);
	}
	msg[msg_len++] = b;
	if (msg_len >= msg_need) {
		SDL_ANDROID_MidiSend(msg, msg_len);
		msg_len = 0;
	}
}

bool midi_host_in(uae_u8 *b)
{
	if (!midi_open)
		return false;
	return SDL_ANDROID_MidiReceive(b, 1) == 1;
}

#else

bool midi_host_open(void)
{
	return false;
}

void midi_host_close(void)
{
}

bool midi_host_is_open(void)
{
	return false;
}

void midi_host_device(bool input, TCHAR *name, int size)
{
	name[0] = 0;
}

void midi_host_out(uae_u8 b)
{
}

bool midi_host_in(uae_u8 *b)
{
	return false;
}

#endif
