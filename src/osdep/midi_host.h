/*
 * The host's MIDI, reached by the Amiga through its serial port.
 *
 * An Amiga MIDI interface sits on the serial port at 31250 baud and passes
 * the bytes through as they are, so the serial emulation hands each byte it
 * sends to the host and takes the bytes the host receives.
 */
#ifndef MIDI_HOST_H
#define MIDI_HOST_H

/* Open the host side; false if there is none. Opening or closing again does
   nothing. */
extern bool midi_host_open(void);
extern void midi_host_close(void);
extern bool midi_host_is_open(void);

/* The name of what the bytes go to (input false) or come from (input true),
   an empty string while there is nothing */
extern void midi_host_device(bool input, TCHAR *name, int size);

/* One byte of the MIDI stream the Amiga sends */
extern void midi_host_out(uae_u8 b);

/* The next byte of the MIDI stream received from the host, if there is one */
extern bool midi_host_in(uae_u8 *b);

#endif /* MIDI_HOST_H */
