 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Serial Line Emulation
  *
  * Copyright 1996, 1997 Stefan Reinauer <stepan@linux.de>
  * Copyright 1997 Christian Schmitt <schmitt@freiburg.linux.de>
  */

#ifndef UAE_SERIAL_H
#define UAE_SERIAL_H

#include "uae/types.h"

extern uae_u16 SERDATR (void);
extern void  SERPER (uae_u16 w);
extern void  SERDAT (uae_u16 w);

extern uae_u16 serdat;

extern void serial_hsynchandler (void);

#endif /* UAE_SERIAL_H */
