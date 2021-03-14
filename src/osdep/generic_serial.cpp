/*
* UAE - The Un*x Amiga Emulator
*
*  Serial Line Emulation
*
* (c) 1996, 1997 Stefan Reinauer <stepan@linux.de>
* (c) 1997 Christian Schmitt <schmitt@freiburg.linux.de>
*
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory-uae.h"
#include "custom.h"
#include "newcpu.h"
#include "cia.h"
#include "serial.h"

static int data_in_serdat; /* new data written to SERDAT */
static int data_in_sershift; /* data transferred from SERDAT to shift register */
static uae_u16 serdatshift; /* serial shift register */
static int serial_period_hsyncs, serial_period_hsync_counter;
static int ninebit;
static int lastbitcycle_active_hsyncs;
static unsigned int lastbitcycle;

uae_u16 serper, serdat;

void SERPER (uae_u16 w)
{
	if (serper == w)  /* don't set baudrate if it's already ok */
		return;

	ninebit = 0;
	serper = w;
	if (w & 0x8000)
		ninebit = 1;

	serial_period_hsyncs = (((serper & 0x7fff) + 1) * (1 + 8 + ninebit + 1 - 1)) / maxhpos;
	if (serial_period_hsyncs <= 0)
		serial_period_hsyncs = 1;

	serial_period_hsync_counter = 0;
}

static void serdatcopy(void);

static void checksend(void)
{
	if (data_in_sershift != 1 && data_in_sershift != 2)
		return;

	if (serial_period_hsyncs <= 1 || data_in_sershift == 2) {
		data_in_sershift = 0;
		serdatcopy();
	} else {
		data_in_sershift = 3;
	}
}

static bool checkshiftempty(void)
{
	checksend();
	if (data_in_sershift == 3) {
		data_in_sershift = 0;
		serdatcopy();
		return true;
	}
	return false;
}

static void sersend_ce(uae_u32 v)
{
	if (checkshiftempty()) {
		lastbitcycle = get_cycles() + ((serper & 0x7fff) + 1) * CYCLE_UNIT;
		lastbitcycle_active_hsyncs = ((serper & 0x7fff) + 1) / maxhpos + 2;
	} else if (data_in_sershift == 1 || data_in_sershift == 2) {
		event2_newevent_x_replace(maxhpos, 0, sersend_ce);
	}
}

static void serdatcopy(void)
{
	int bits;

	if (data_in_sershift || !data_in_serdat)
		return;
	serdatshift = serdat;
	bits = 8;
	if ((serdatshift & 0xff80) == 0x80) {
		bits = 7;
	}
	data_in_sershift = 1;
	data_in_serdat = 0;

	// if someone uses serial port as some kind of timer..
	if (currprefs.cpu_memory_cycle_exact) {
		int per;

		bits = 16 + 1;
		for (int i = 15; i >= 0; i--) {
			if (serdatshift & (1 << i))
				break;
			bits--;
		}
		// assuming when last bit goes out, transmit buffer
		// becomes empty, not when last bit has finished
		// transmitting.
		per = ((serper & 0x7fff) + 1) * (bits - 1);
		if (lastbitcycle_active_hsyncs) {
			// if last bit still transmitting, add remaining time.
			int extraper = lastbitcycle - get_cycles();
			if (extraper > 0)
				per += extraper / CYCLE_UNIT;
		}
		if (per < 4)
			per = 4;
		event2_newevent_x_replace(per, 0, sersend_ce);
	}

	INTREQ(0x8000 | 0x0001);
	checksend();
}

void serial_hsynchandler (void)
{
	if (lastbitcycle_active_hsyncs > 0)
		lastbitcycle_active_hsyncs--;
	if (serial_period_hsyncs == 0)
		return;
	serial_period_hsync_counter++;
	if (serial_period_hsyncs == 1 || (serial_period_hsync_counter % (serial_period_hsyncs - 1)) == 0) {
		checkshiftempty();
	} else if ((serial_period_hsync_counter % serial_period_hsyncs) == 0 && !currprefs.cpu_cycle_exact) {
		checkshiftempty();
	}
}

void SERDAT (uae_u16 w)
{
	serdatcopy();

	serdat = w;

	if (!w) {
		return;
	}

	data_in_serdat = 1;
	serdatcopy();
}

uae_u16 SERDATR(void)
{
  uae_u16 serdatr = 0;
	if (!data_in_serdat) {
		serdatr |= 0x2000;
	}
	if (!data_in_sershift) {
		serdatr |= 0x1000;
	}
	serdatr |= 0x0800;
	return serdatr;
}
