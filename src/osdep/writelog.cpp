 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Standard write_log that writes to the console
  *
  * Copyright 2001 Bernd Schmidt
  */
#include "sysdeps.h"
#include "uae.h"

#ifdef ANDROID
#include <android/log.h>
// stdout is discarded on Android, so printf() based logging is invisible.
#define UAE_LOG(buf) __android_log_print(ANDROID_LOG_INFO, "uae4arm", "%s", buf)
#else
// Flushed at once: with the output going to a file, as when running under
// gdb, the last lines before a hang or crash would otherwise be lost.
#define UAE_LOG(buf) do { fputs(buf, stdout); fflush(stdout); } while (0)
#endif

#define WRITE_LOG_BUF_SIZE 4096
FILE *debugfile = NULL;

void console_out (const TCHAR *format,...)
{
    va_list parms;
    TCHAR buffer[WRITE_LOG_BUF_SIZE];

    va_start (parms, format);
    vsnprintf (buffer, WRITE_LOG_BUF_SIZE-1, format, parms);
    va_end (parms);
    UAE_LOG(buffer);
}

#ifdef WITH_LOGGING

void write_log (const TCHAR *format,...)
{
  int count;
  int numwritten;
  TCHAR buffer[WRITE_LOG_BUF_SIZE];

  va_list parms;
  va_start (parms, format);
  count = vsnprintf( buffer, WRITE_LOG_BUF_SIZE-1, format, parms );
  if( debugfile ) {
	  fprintf( debugfile, "%s", buffer );
	  fflush (debugfile);
  }
  UAE_LOG(buffer);
  va_end (parms);
}

#endif

void jit_abort (const TCHAR *format,...)
{
    static int happened;
    int count;
    TCHAR buffer[WRITE_LOG_BUF_SIZE];
    va_list parms;
    va_start (parms, format);

    count = vsnprintf (buffer, WRITE_LOG_BUF_SIZE - 1, format, parms);
    write_log (buffer);
    va_end (parms);
    if (!happened)
	gui_message ("JIT: Serious error:\n%s", buffer);
    happened = 1;
    uae_reset (1, 0);
}
