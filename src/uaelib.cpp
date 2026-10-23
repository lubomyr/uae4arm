/*
 * UAE - The U*nix Amiga Emulator
 *
 * UAE Library v0.1
 *
 * (c) 1996 Tauno Taipaleenmaki <tataipal@raita.oulu.fi>
 *
 * Change UAE parameters and other stuff from inside the emulation.
 */

#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory-uae.h"
#include "autoconf.h"
#include "disk.h"
#include "gensound.h"
#include "picasso96.h"
#include "filesys.h"
#if defined(ANDROIDSDL)
#include <SDL_android.h>
#elif defined(LINUX_HOST)
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <dirent.h>

/*
 * Returns UAE Version
 */
static uae_u32 emulib_GetVersion (void)
{
  return version;
}

/*
 * Resets your amiga
 */
static uae_u32 emulib_HardReset (void)
{
	uae_reset(1, 1);
  return 0;
}

static uae_u32 emulib_Reset (void)
{
	uae_reset(0, 0);
  return 0;
}

/*
 * Enables SOUND
 */
static uae_u32 emulib_EnableSound (uae_u32 val)
{
  if (!sound_available || currprefs.produce_sound == 2)
  	return 0;

  currprefs.produce_sound = val;
  return 1;
}

/*
 * Enables FAKE JOYSTICK
 */
static uae_u32 emulib_EnableJoystick (uae_u32 val)
{
	currprefs.jports[0].id = val & 255;
	currprefs.jports[1].id = (val >> 8) & 255;
	return 1;
}

/*
 * Sets the framerate
 */
static uae_u32 emulib_SetFrameRate (uae_u32 val)
{
  if (val == 0)
  	return 0;
  else if (val > 20)
  	return 0;
  else {
  	currprefs.gfx_framerate = val;
  	return 1;
  }
}

/*
 * Changes keyboard language settings
 */
static uae_u32 emulib_ChangeLanguage (uae_u32 which)
{
	if (which > 6)
    return 0;
	else {
		switch (which) {
		  case 0:
			  currprefs.keyboard_lang = KBD_LANG_US;
			  break;
		  case 1:
			  currprefs.keyboard_lang = KBD_LANG_DK;
			  break;
		  case 2:
			  currprefs.keyboard_lang = KBD_LANG_DE;
			  break;
		  case 3:
			  currprefs.keyboard_lang = KBD_LANG_SE;
			  break;
		  case 4:
			  currprefs.keyboard_lang = KBD_LANG_FR;
			  break;
		  case 5:
			  currprefs.keyboard_lang = KBD_LANG_IT;
			  break;
		  case 6:
			  currprefs.keyboard_lang = KBD_LANG_ES;
			  break;
		  default:
			  break;
		}
    return 1;
  }
}

/* The following ones don't work as we never realloc the arrays... */
/*
 * Changes chip memory size
 *  (reboots)
 */
static uae_u32 REGPARAM2 emulib_ChgCMemSize(TrapContext *ctx, uae_u32 memsize)
{
  if (memsize != 0x80000 && memsize != 0x100000 &&
  	memsize != 0x200000) {
    	memsize = 0x200000;
    	write_log (_T("Unsupported chipmem size!\n"));
  }
	trap_set_dreg(ctx, 0,  0);

  changed_prefs.chipmem.size = memsize;
	uae_reset(1, 1);
  return 1;
}

/*
 * Changes slow memory size
 *  (reboots)
 */
static uae_u32 REGPARAM2 emulib_ChgSMemSize(TrapContext *ctx, uae_u32 memsize)
{
  if (memsize != 0x80000 && memsize != 0x100000 &&
  	memsize != 0x180000 && memsize != 0x1C0000) {
  	  memsize = 0;
			write_log (_T("Unsupported bogomem size!\n"));
  }

	trap_set_dreg(ctx, 0, 0);
  changed_prefs.bogomem.size = memsize;
	uae_reset (1, 1);
  return 1;
}

/*
 * Changes fast memory size
 *  (reboots)
 */
static uae_u32 REGPARAM2 emulib_ChgFMemSize(TrapContext *ctx, uae_u32 memsize)
{
  if (memsize != 0x100000 && memsize != 0x200000 &&
	  memsize != 0x400000 && memsize != 0x800000) {
    	memsize = 0;
			write_log (_T("Unsupported fastmem size!\n"));
  }
	trap_set_dreg(ctx, 0, 0);
  changed_prefs.fastmem[0].size = memsize;
	uae_reset (1, 1);
  return 0;
}

/*
 * Inserts a disk
 */
static uae_u32 emulib_InsertDisk(TrapContext *ctx, uaecptr name, uae_u32 drive)
{
  char real_name[256];
	TCHAR *s;

  if (drive > 3)
  	return 0;

	if (trap_get_string(ctx, real_name, name, sizeof real_name) >= sizeof real_name)
  	return 0; /* ENAMETOOLONG */

	s = au (real_name);
	_tcscpy (changed_prefs.floppyslots[drive].df, s);
	xfree (s);

  return 1;
}

/*
 * Exits the emulator
 */
static uae_u32 emulib_ExitEmu (void)
{
  gui_message(_T("Amiga sent signal to quit emulator."));
  uae_restart(1, NULL);
  return 1;
}

/*
 * Gets UAE Configuration
 */
static uae_u32 emulib_GetUaeConfig(TrapContext *ctx, uaecptr place)
{
	trap_put_long(ctx, place, version);
	trap_put_long(ctx, place + 4, chipmem_bank.allocated_size);
	trap_put_long(ctx, place + 8, bogomem_bank.allocated_size);
	trap_put_long(ctx, place + 12, fastmem_bank[0].allocated_size);
	trap_put_long(ctx, place + 16, currprefs.gfx_framerate + 1);
	trap_put_long(ctx, place + 20, currprefs.produce_sound);
	trap_put_long(ctx, place + 24, currprefs.jports[0].id | (currprefs.jports[1].id << 8));
	trap_put_long(ctx, place + 28, currprefs.keyboard_lang);
  if (disk_empty (0))
		trap_put_byte(ctx, place + 32, 0);
  else
		trap_put_byte(ctx, place + 32, 1);
  if (disk_empty (1))
		trap_put_byte(ctx, place + 33, 0);
  else
		trap_put_byte(ctx, place + 33, 1);
  if (disk_empty(2))
		trap_put_byte(ctx, place + 34, 0);
  else
		trap_put_byte(ctx, place + 34, 1);
  if (disk_empty(3))
		trap_put_byte(ctx, place + 35, 0);
  else
		trap_put_byte(ctx, place + 35, 1);

	for (int i = 0; i < 4; i++) {
		char *s = ua (currprefs.floppyslots[i].df);
		trap_put_string(ctx, s, place + 36 + i * 256, 256);
		xfree (s);
  }
  return 1;
}

/*
 * Sets UAE Configuration
 *
 * NOT IMPLEMENTED YET
 */
static uae_u32 emulib_SetUaeConfig (uaecptr place)
{
  return 1;
}

/*
 * Gets the name of the disk in the given drive
 */
static uae_u32 emulib_GetDisk(TrapContext *ctx, uae_u32 drive, uaecptr name)
{
  if (drive > 3)
  	return 0;

	char *n = ua(currprefs.floppyslots[drive].df);
	trap_put_string(ctx, (uae_u8*)n, name, 256);
	xfree(n);
  return 1;
}

static int native_dos_op(TrapContext *ctx, uae_u32 mode, uae_u32 p1, uae_u32 p2, uae_u32 p3)
{
  TCHAR tmp[MAX_DPATH];
	char *s;
	int v;

  if (mode)
  	return -1;
  /* receive native path from lock
   * p1 = dos.library:Lock, p2 = buffer, p3 = max buffer size 
   */
	v = get_native_path(ctx, p1, tmp);
  if (v)
  	return v;
	s = ua (tmp);
	trap_put_string(ctx, (uae_u8*)s, p2, p3);
	xfree (s);
  return 0;
}

#if defined(LINUX_HOST)
/* The desktop's own program for a file or an address, detached so that the
   emulation does not wait */
static bool host_open(const TCHAR *what)
{
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execlp("xdg-open", "xdg-open", what, (char *)NULL);
			_exit(127);
		}
		_exit(0);
	}
	if (pid < 0)
		return false;
	waitpid(pid, NULL, 0);
	return true;
}
#endif

/* A video of the Amiga's own drives - a hardfile, a floppy, RAM: - which the
   host cannot reach: AndroidPlay hands its bytes over, into a copy in the
   app's folder, and the host plays the copy. The video window deletes it when
   closed or given another video, and the app the ones left at its start. */
static FILE *copy_file = NULL;
static char copy_path[MAX_DPATH];

static void copy_drop(void)
{
	if (copy_file) {
		fclose(copy_file);
		copy_file = NULL;
		unlink(copy_path);
	}
}

/* The copies, at the app's end: it ends with _exit(), so its video window
   does not delete its copy. A video still playing then goes with the app. */
void emulib_host_cleanup(void)
{
#if defined(ANDROIDSDL)
	const char *dir = getenv("APPDIR");
	if (!dir)
		return;
	copy_drop();
	char folder[MAX_DPATH];
	snprintf(folder, sizeof folder, "%s/AndroidPlay", dir);
	DIR *d = opendir(folder);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.')
			continue;
		char file[MAX_DPATH];
		snprintf(file, sizeof file, "%s/%s", folder, e->d_name);
		unlink(file);
	}
	closedir(d);
#endif
}

/* The Amiga starts again - a reset, a restart, another configuration: the
   windows its programs opened on the host close, and a copy being made goes */
void emulib_host_reset(void)
{
	copy_drop();
#if defined(ANDROIDSDL)
	SDL_ANDROID_CloseFloatingWindows();
#endif
}

/* Starts a copy, named for the extension of the Amiga file's name */
static bool copy_begin(const char *name)
{
	static unsigned int count;
	const char *dir = NULL;

	copy_drop();
#if defined(ANDROIDSDL)
	dir = getenv("APPDIR");
#endif
	char folder[MAX_DPATH];
	if (dir)
		snprintf(folder, sizeof folder, "%s/AndroidPlay", dir);
	else
		snprintf(folder, sizeof folder, "/tmp/uae4arm-AndroidPlay");
	mkdir(folder, 0700);
	const char *ext = strrchr(name, '.');
	if (!ext || strchr(ext, '/') || strchr(ext, ':') || strlen(ext) > 8)
		ext = "";
	snprintf(copy_path, sizeof copy_path, "%s/%lu-%u%s", folder, (unsigned long)time(NULL), ++count, ext);
	copy_file = fopen(copy_path, "wb");
	write_log(_T("uaelib: copying %s into %s\n"), name, copy_file ? copy_path : "nothing");
	return copy_file != NULL;
}

static bool copy_piece(TrapContext *ctx, uaecptr addr, uae_u32 len)
{
	if (!copy_file || len > 16 * 1024 * 1024 || !valid_address(addr, len))
		return false;
	uae_u8 *buf = xmalloc(uae_u8, len);
	if (!buf)
		return false;
	trap_get_bytes(ctx, buf, addr, len);
	bool ok = fwrite(buf, 1, len, copy_file) == len;
	xfree(buf);
	return ok;
}

/* uae4arm: the host shows something in a window over the Amiga screen, for
   AndroidPlay and AndroidBrowser on the Amiga side. what and arg:
   1 plays a video, arg = dos.library Lock of a file of a folder drive;
   2 shows a web page, arg = its address, a string - empty asks for one;
   3 shows a file of a folder drive as a web page, arg = its Lock;
   a video of another drive is copied: 4 starts a copy, arg = the file's name,
   5 adds arg2 bytes at arg to it, 6 plays it, 7 drops it;
   8 shows a picture, arg = its Lock; 9 shows a copy as a picture.
   Above the low byte of what: $100 leaves out the address field of the web
   page's window, $200 shows the page in a window of its own.
   1 done, 2 not a file of a folder drive, 3 the host cannot show it - not 0,
   which is what an emulator without this gives back. */
#if defined(ANDROIDSDL)
static int browser_flags(int noaddress, int newwindow)
{
	return (noaddress ? SDL_ANDROID_BROWSER_NO_ADDRESS : 0) | (newwindow ? SDL_ANDROID_BROWSER_NEW_WINDOW : 0);
}
#endif

static uae_u32 emulib_HostShow(TrapContext *ctx, uae_u32 what, uae_u32 arg, uae_u32 arg2)
{
	TCHAR path[MAX_DPATH];
	int noaddress = (what & 0x100) != 0;
	int newwindow = (what & 0x200) != 0;

	(void)noaddress; // only the Android window has the field, and more windows
	(void)newwindow;
	what &= 0xff;
	if (what == 4) {
		char name[MAX_DPATH];
		trap_get_string(ctx, name, arg, sizeof name);
		name[sizeof name - 1] = 0;
		return copy_begin(name) ? 1 : 3;
	}
	if (what == 5) {
		if (copy_piece(ctx, arg, arg2))
			return 1;
		write_log(_T("uaelib: the copy failed\n"));
		copy_drop();
		return 3;
	}
	if (what == 7) {
		copy_drop();
		return 1;
	}
	if (what == 6 || what == 9) {
		if (!copy_file)
			return 3;
		bool ok = fclose(copy_file) == 0;
		copy_file = NULL;
		if (!ok) {
			unlink(copy_path);
			return 3;
		}
		write_log(_T("uaelib: the host shows %s\n"), copy_path);
#if defined(ANDROIDSDL)
		if (what == 9)
			return SDL_ANDROID_ShowImage(copy_path) ? 1 : 3;
		return SDL_ANDROID_PlayVideo(copy_path) ? 1 : 3;
#elif defined(LINUX_HOST)
		return host_open(copy_path) ? 1 : 3;
#else
		return 3;
#endif
	}
	if (what == 2) {
		char url[2048];
		trap_get_string(ctx, url, arg, sizeof url);
		url[sizeof url - 1] = 0;
		write_log(_T("uaelib: the host shows %s\n"), url);
#if defined(ANDROIDSDL)
		return SDL_ANDROID_OpenBrowser(url, 0, browser_flags(noaddress, newwindow)) ? 1 : 3;
#elif defined(LINUX_HOST)
		if (!url[0])
			return 3;
		if (strstr(url, "://"))
			return host_open(url) ? 1 : 3;
		{
			std::string https = std::string("https://") + url;
			return host_open(https.c_str()) ? 1 : 3;
		}
#else
		return 3;
#endif
	}
	if (what != 1 && what != 3 && what != 8)
		return 3;
	if (get_native_path(ctx, arg, path))
		return 2;
	write_log(_T("uaelib: the host shows %s\n"), path);
#if defined(ANDROIDSDL)
	if (what == 1)
		return SDL_ANDROID_PlayVideo(path) ? 1 : 3;
	if (what == 8)
		return SDL_ANDROID_ShowImage(path) ? 1 : 3;
	return SDL_ANDROID_OpenBrowser(path, 1, browser_flags(noaddress, newwindow)) ? 1 : 3;
#elif defined(LINUX_HOST)
	return host_open(path) ? 1 : 3;
#else
	return 3;
#endif
}

static uae_u32 uaelib_demux_common(TrapContext *ctx, uae_u32 ARG0, uae_u32 ARG1, uae_u32 ARG2, uae_u32 ARG3, uae_u32 ARG4, uae_u32 ARG5)
{
	switch (ARG0) {
    case 0: return emulib_GetVersion ();
		case 1: return emulib_GetUaeConfig(ctx, ARG1);
    case 2: return emulib_SetUaeConfig (ARG1);
    case 3: return emulib_HardReset ();
    case 4: return emulib_Reset ();
		case 5: return emulib_InsertDisk(ctx, ARG1, ARG2);
    case 6: return emulib_EnableSound (ARG1);
    case 7: return emulib_EnableJoystick (ARG1);
    case 8: return emulib_SetFrameRate (ARG1);
		case 9: return emulib_ChgCMemSize(ctx, ARG1);
		case 10: return emulib_ChgSMemSize(ctx, ARG1);
		case 11: return emulib_ChgFMemSize(ctx, ARG1);
    case 12: return emulib_ChangeLanguage (ARG1);
   	  /* The next call brings bad luck */
    case 13: return emulib_ExitEmu ();
		case 14: return emulib_GetDisk(ctx, ARG1, ARG2);
    case 80: 
      return 0xffffffff;
		case 81: return cfgfile_uaelib(ctx, ARG1, ARG2, ARG3, ARG4);
		case 82: return cfgfile_uaelib_modify(ctx, ARG1, ARG2, ARG3, ARG4, ARG5);
		case 85: return native_dos_op(ctx, ARG1, ARG2, ARG3, ARG4);
		case 200: return emulib_HostShow(ctx, ARG1, ARG2, ARG3);
    case 86:
   	  if (valid_address(ARG1, 1)) {
			  uae_char tmp[MAX_DPATH];
			  trap_get_string(ctx, tmp, ARG1, sizeof tmp);
			  TCHAR *s = au(tmp);
			  write_log (_T("DBG: %s\n"), s);
			  xfree (s);
   	    return 1;
      } 
      return 0;
    case 87:
	    {
	      uae_u32 d0, d1;
	      d0 = emulib_target_getcpurate (ARG1, &d1);
			  trap_set_dreg(ctx, 1, d1);
	      return d0;
	    }
  }
  return 0;
}

static uae_u32 REGPARAM2 uaelib_demux2 (TrapContext *ctx)
{
#define ARG0 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 4))
#define ARG1 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 8))
#define ARG2 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 12))
#define ARG3 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 16))
#define ARG4 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 20))
#define ARG5 (trap_get_long(ctx, trap_get_areg(ctx, 7) + 24))

#ifdef PICASSO96
	if (ARG0 >= 16 && ARG0 <= 39)
		return picasso_demux(ARG0, ctx);
#endif
	return uaelib_demux_common(ctx, ARG0, ARG1, ARG2, ARG3, ARG4, ARG5);
}

static uae_u32 REGPARAM2 uaelib_demux (TrapContext *ctx)
{
  uae_u32 v;

  v = uaelib_demux2 (ctx);
  return v;
}

/*
 * Installs the UAE LIBRARY
 */
void emulib_install (void)
{
  uaecptr a;
	if (!uae_boot_rom_type && !currprefs.uaeboard)
  	return;
  a = here ();
  org (rtarea_base + 0xFF60);
  calltrap (deftrapres (uaelib_demux, 0, _T("uaelib_demux")));
  dw (RTS);
  org (a);
}
