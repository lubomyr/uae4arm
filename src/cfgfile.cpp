 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Config file handling
  * This still needs some thought before it's complete...
  *
  * Copyright 1998 Brian King, Bernd Schmidt
  */

#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "audio.h"
#include "custom.h"
#include "inputdevice.h"
#include "savestate.h"
#include "memory-uae.h"
#include "autoconf.h"
#include "rommgr.h"
#include "gui.h"
#include "newcpu.h"
#include "zfile.h"
#include "filesys.h"
#include "fsdb.h"
#include "disk.h"
#include "blkdev.h"
#include "statusline.h"
#include "calc.h"
#include "gfxboard.h"

#define cfgfile_warning write_log
#define cfgfile_warning_obsolete write_log

static int config_newfilesystem;
static struct strlist *temp_lines;
static struct zfile *default_file, *configstore;
static int uaeconfig;

static const TCHAR *guimode1[] = { _T("no"), _T("yes"), _T("nowait"), 0 };
static const TCHAR *guimode2[] = { _T("false"), _T("true"), _T("nowait"), 0 };
static const TCHAR *guimode3[] = { _T("0"), _T("1"), _T("nowait"), 0 };
static const TCHAR *csmode[] = { _T("ocs"), _T("ecs_agnus"), _T("ecs_denise"), _T("ecs"), _T("aga"), 0 };
static const TCHAR *linemode[] = {
	_T("none"),
	_T("double"),
	0 };
static const TCHAR *speedmode[] = { _T("max"), _T("real"), 0 };
static const TCHAR *soundmode1[] = { _T("none"), _T("interrupts"), _T("normal"), _T("exact"), 0 };
static const TCHAR *soundmode2[] = { _T("none"), _T("interrupts"), _T("good"), _T("best"), 0 };
static const TCHAR *stereomode[] = { _T("mono"), _T("stereo"),  0 };
static const TCHAR *interpolmode[] = { _T("none"), _T("anti"), _T("sinc"), _T("rh"), _T("crux"), 0 };
static const TCHAR *collmode[] = { _T("none"), _T("sprites"), _T("playfields"), _T("full"), 0 };
static const TCHAR *soundfiltermode1[] = { _T("off"), _T("emulated"), _T("on"), 0 };
static const TCHAR *soundfiltermode2[] = { _T("standard"), _T("enhanced"), 0 };
static const TCHAR *lorestype1[] = { _T("lores"), _T("hires"), _T("superhires"), 0 };
static const TCHAR *lorestype2[] = { _T("true"), _T("false"), 0 };
static const TCHAR *cartsmode[] = { _T("none"), _T("hrtmon"), 0 };
static const TCHAR *idemode[] = { _T("none"), _T("a600/a1200"), _T("a4000"), 0 };
static const TCHAR *rtctype[] = { _T("none"), _T("MSM6242B"), _T("RP5C01A"), _T("MSM6242B_A2000"), 0 };
static const TCHAR *ciaatodmode[] = { _T("vblank"), _T("50hz"), _T("60hz"), 0 };
static const TCHAR *cscompa[] = {
	_T("-"), _T("Generic"), _T("CD32"), _T("A500"), _T("A500+"), _T("A600"),
	_T("A1200"), _T("A2000"), _T("A4000"), 
	NULL
};
static const TCHAR *qsmodes[] = {
	_T("A500"), _T("A500+"), _T("A600"), _T("A1200"), _T("A4000"), _T("CD32"), NULL };
static const TCHAR *scsimode[] = { _T("false"), _T("true"), _T("scsi"), 0 };
static const TCHAR *abspointers[] = { _T("none"), _T("mousehack"), 0 };
static const TCHAR *joyportmodes[] = { _T(""), _T("mouse"), _T("djoy"), _T("gamepad"), _T("ajoy"), _T("cd32joy"), 0 };
static const TCHAR *joyaf[] = { _T("none"), _T("normal"), _T("toggle"), _T("always"), 0 };
static const TCHAR *cdmodes[] = { _T("disabled"), _T(""), _T("image"), 0 };
static const TCHAR *waitblits[] = { _T("disabled"), _T("automatic"), _T("noidleonly"), _T("always"), 0 };
static const TCHAR *autoext2[] = { _T("disabled"), _T("copy"), _T("replace"), 0 };
static const TCHAR *leds[] = { _T("power"), _T("df0"), _T("df1"), _T("df2"), _T("df3"), _T("hd"), _T("cd"), _T("fps"), _T("cpu"), _T("snd"), _T("md"), _T("net"), 0 };
static const int leds_order[] = { 3, 6, 7, 8, 9, 4, 5, 2, 1, 0, 9, 10 };
/* another boolean to choice update.. */
static const TCHAR *cycleexact[] = { _T("false"), _T("memory"), _T("true"), 0  };
static const TCHAR *unmapped[] = { _T("floating"), _T("zero"), _T("one"), 0 };
static const TCHAR *ciatype[] = { _T("default"), _T("391078-01"), 0 };

struct hdcontrollerconfig
{
	const TCHAR *label;
	int romtype;
};

static const struct hdcontrollerconfig hdcontrollers[] = {
	{ _T("uae"), 0 },

	{ _T("ide%d"), 0 },
	{ _T("ide%d_mainboard"), ROMTYPE_MB_IDE },

	{ NULL }
};
static const TCHAR *z3mapping[] = {
	_T("auto"),
	_T("uae"),
	_T("real"),
	NULL
};
static const TCHAR *uaebootrom[] = {
	_T("automatic"),
	_T("disabled"), 
	_T("min"),
	_T("full"),
	NULL
};
static const TCHAR *uaeboard[] = {
	_T("disabled"),
	_T("min"),
	NULL
};

static const TCHAR *obsolete[] = {
	_T("accuracy"), _T("gfx_opengl"), _T("gfx_32bit_blits"), _T("32bit_blits"),
	_T("gfx_immediate_blits"), _T("gfx_ntsc"), _T("win32"), _T("gfx_filter_bits"),
	_T("sound_pri_cutoff"), _T("sound_pri_time"), _T("sound_min_buff"), _T("sound_bits"),
	_T("gfx_test_speed"), _T("gfxlib_replacement"), _T("enforcer"), _T("catweasel_io"),
  _T("kickstart_key_file"), _T("sound_adjust"), _T("sound_latency"),
	_T("serial_hardware_dtrdsr"), _T("gfx_filter_upscale"),
	_T("gfx_correct_aspect"), _T("gfx_autoscale"), _T("parallel_sampler"), _T("parallel_ascii_emulation"),
	_T("avoid_vid"), _T("avoid_dga"), _T("z3chipmem_size"), _T("state_replay_buffer"), _T("state_replay"),
	_T("z3realmapping"), _T("force_0x10000000_z3"),
	_T("fpu_arithmetic_exceptions"),

	_T("gfx_filter_vert_zoom"),_T("gfx_filter_horiz_zoom"),
	_T("gfx_filter_vert_zoom_mult"), _T("gfx_filter_horiz_zoom_mult"),
	_T("gfx_filter_vert_offset"), _T("gfx_filter_horiz_offset"),
	_T("gfx_tearing"), _T("gfx_tearing_rtg"),

	_T("pcibridge_rom_file"),
	_T("pcibridge_rom_options"),

	_T("cpuboard_ext_rom_file"),
	_T("uaeboard_mode"),

	_T("comp_oldsegv"),
	_T("comp_midopt"),
	_T("comp_lowopt"),
	_T("avoid_cmov"),
	_T("compforcesettings"),

  NULL
};

#define UNEXPANDED _T("$(FILE_PATH)")

static TCHAR *cfgfile_unescape(const TCHAR *s, const TCHAR **endpos, TCHAR separator, bool min)
{
	bool quoted = false;
	TCHAR *s2 = xmalloc (TCHAR, _tcslen (s) + 1);
	TCHAR *p = s2;
	if (s[0] == '\"') {
		s++;
		quoted = true;
	}
	int i;
	for (i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (quoted && c == '\"') {
			i++;
			break;
		}
		if (c == separator) {
			i++;
			break;
		}
		if (c == '\\' && !min) {
			char v = 0;
			TCHAR c2;
			c = s[i + 1];
			switch (c)
			{
				case 'X':
				case 'x':
				c2 = _totupper (s[i + 2]);
				v = ((c2 >= 'A') ? c2 - 'A' : c2 - '0') << 4;
				c2 = _totupper (s[i + 3]);
				v |= (c2 >= 'A') ? c2 - 'A' : c2 - '0';
				*p++ = c2;
				i += 2;
				break;
				case 'r':
				*p++ = '\r';
				break;
				case '\n':
				*p++ = '\n';
				break;
				default:
				*p++ = c;
				break;
			}
			i++;
		}
		else {
			*p++ = c;
		}
	}
	*p = 0;
	if (endpos)
		*endpos = &s[i];
	return s2;
}
static TCHAR *cfgfile_unescape (const TCHAR *s, const TCHAR **endpos)
{
	return cfgfile_unescape(s, endpos, 0, false);
}
static TCHAR *cfgfile_unescape_min(const TCHAR *s)
{
	return cfgfile_unescape(s, NULL, 0, true);
}

static TCHAR *cfgfile_option_find_it(const TCHAR *s, const TCHAR *option, bool checkequals)
{
	TCHAR buf[MAX_DPATH];
	if (!s)
		return NULL;
	_tcscpy(buf, s);
	_tcscat(buf, _T(","));
	TCHAR *p = buf;
	for (;;) {
		TCHAR *tmpp = _tcschr(p, ',');
		TCHAR *tmpp2 = NULL;
		if (tmpp == NULL)
			return NULL;
		*tmpp++ = 0;
		if (checkequals) {
			tmpp2 = _tcschr(p, '=');
			if (tmpp2)
				*tmpp2++ = 0;
		}
		if (!_tcsicmp(p, option)) {
			if (checkequals && tmpp2) {
				if (tmpp2[0] == '"') {
					TCHAR *n = cfgfile_unescape_min(tmpp2);
					return n;
				}
				return my_strdup(tmpp2);
			}
			return my_strdup(p);
		}
		p = tmpp;
	}
}

static bool cfgfile_option_find(const TCHAR *s, const TCHAR *option)
{
	TCHAR *ss = cfgfile_option_find_it(s, option, false);
	xfree(ss);
	return ss != NULL;
}

static TCHAR *cfgfile_option_get(const TCHAR *s, const TCHAR *option)
{
	return cfgfile_option_find_it(s, option, true);
}

static void trimwsa (char *s)
{
  /* Delete trailing whitespace.  */
  int len = strlen (s);
  while (len > 0 && strcspn (s + len - 1, "\t \r\n") == 0)
    s[--len] = '\0';
}

static int match_string (const TCHAR *table[], const TCHAR *str)
{
  int i;
  for (i = 0; table[i] != 0; i++)
  	if (_tcsicmp (table[i], str) == 0)
	    return i;
  return -1;
}

// escape config file separators and control characters
static TCHAR *cfgfile_escape (const TCHAR *s, const TCHAR *escstr, bool quote)
{
	bool doquote = false;
	int cnt = 0;
	for (int i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (c == 0)
			break;
		if (c < 32 || c == '\\' || c == '\"' || c == '\'') {
			cnt++;
		}
		for (int j = 0; escstr && escstr[j]; j++) {
			if (c == escstr[j]) {
				cnt++;
				if (quote) {
					doquote = true;
					cnt++;
				}
			}
		}
		// always quote if starts or ends with space
		if (c == ' ' && (s[i + 1] == 0 || i == 0)) {
			doquote = true;
		}
	}
	if (escstr == NULL && quote)
		doquote = true;
	TCHAR *s2 = xmalloc (TCHAR, _tcslen (s) + cnt * 4 + 2 + 1);
	TCHAR *p = s2;
	if (doquote)
		*p++ = '\"';
	for (int i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (c == 0)
			break;
		if (c == '\\' || c == '\"' || c == '\'') {
			*p++ = '\\';
			*p++ = c;
		} else if (c >= 32 && !quote) {
			bool escaped = false;
			for (int j = 0; escstr && escstr[j]; j++) {
				if (c == escstr[j]) {
					*p++ = '\\';
					*p++ = c;
					escaped = true;
					break;
				}
			}
			if (!escaped)
				*p++ = c;
		} else if (c < 32) {
			*p++ = '\\';
			switch (c)
			{
				case '\t':
				*p++ = 't';
				break;
				case '\n':
				*p++ = 'n';
				break;
				case '\r':
				*p++ = 'r';
				break;
				default:
				*p++ = 'x';
				*p++ = (c >> 4) >= 10 ? (c >> 4) + 'a' : (c >> 4) + '0';
				*p++ = (c & 15) >= 10 ? (c & 15) + 'a' : (c & 15) + '0';
				break;
			}
		} else {
			*p++ = c;
		}
	}
	if (doquote)
		*p++ = '\"';
	*p = 0;
	return s2;
}

// escape only , and " or if starts or ends with a space
static TCHAR *cfgfile_escape_min(const TCHAR *s)
{
	for (int i = 0; s[i]; i++) {
		TCHAR c = s[i];
		if (c == ',' || c == '\"' || (c == ' ' && (i == 0 || s[i + 1] == 0))) {
			return cfgfile_escape(s, _T(","), true);
		}
	}
	return my_strdup(s);
}

static TCHAR *getnextentry (const TCHAR **valuep, const TCHAR separator)
{
	TCHAR *s;
	const TCHAR *value = *valuep;
	if (value[0] == '\"') {
		s = cfgfile_unescape (value, valuep);
		value = *valuep;
		if (*value != 0 && *value != separator) {
			xfree (s);
			return NULL;
		}
		value++;
		*valuep = value;
	} else {
		s = cfgfile_unescape (value, valuep, separator, false);
	}
	return s;
}

static TCHAR *cfgfile_subst_path2 (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
  /* @@@ use _tcsicmp for some targets.  */
  if (path != NULL && subst != NULL && _tcslen (path) > 0 && _tcsncmp (file, path, _tcslen (path)) == 0) {
  	int l;
  	TCHAR *p2, *p = xmalloc (TCHAR, _tcslen (file) + _tcslen (subst) + 2);
  	_tcscpy (p, subst);
  	l = _tcslen (p);
  	while (l > 0 && p[l - 1] == '/')
	    p[--l] = '\0';
  	l = _tcslen (path);
  	while (file[l] == '/')
	    l++;
		_tcscat (p, _T("/"));
    _tcscat (p, file + l);
		p2 = target_expand_environment (p, NULL, 0);
		xfree (p);
		if (p2 && p2[0] == '$') {
			xfree(p2);
			return NULL;
		}
	  return p2;
  }
	return NULL;
}

static TCHAR *cfgfile_subst_path (const TCHAR *path, const TCHAR *subst, const TCHAR *file)
{
	TCHAR *s = cfgfile_subst_path2 (path, subst, file);
	if (s)
		return s;
	s = target_expand_environment (file, NULL, 0);
	return s;
}

static TCHAR *cfgfile_get_multipath2 (struct multipath *mp, const TCHAR *path, const TCHAR *file, bool dir)
{
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], _T(".\\")) != 0 && _tcscmp (mp->path[i], _T("./")) != 0 && (file[0] != '/' && file[0] != '\\' && !_tcschr(file, ':'))) {
			TCHAR *s = NULL;
			if (path)
				s = cfgfile_subst_path2 (path, mp->path[i], file);
			if (!s) {
				TCHAR np[MAX_DPATH];
				_tcscpy (np, mp->path[i]);
				fixtrailing (np);
				_tcscat (np, file);
				s = my_strdup (np);
			}
			if (dir) {
				if (my_existsdir (s))
					return s;
			} else {
				if (zfile_exists (s))
					return s;
			}
			xfree (s);
		}
	}
	return NULL;
}

static TCHAR *cfgfile_put_multipath (struct multipath *mp, const TCHAR *s)
{
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] && _tcscmp (mp->path[i], _T(".\\")) != 0 && _tcscmp (mp->path[i], _T("./")) != 0) {
			if (_tcsnicmp (mp->path[i], s, _tcslen (mp->path[i])) == 0) {
				return my_strdup (s + _tcslen (mp->path[i]));
			}
		}
	}
	return my_strdup (s);
}


static TCHAR *cfgfile_subst_path_load (const TCHAR *path, struct multipath *mp, const TCHAR *file, bool dir)
{
	TCHAR *s = cfgfile_get_multipath2 (mp, path, file, dir);
	if (s)
		return s;
	return cfgfile_subst_path (path, mp->path[0], file);
}

static bool isdefault (const TCHAR *s)
{
  TCHAR tmp[MAX_DPATH];
  if (!default_file || uaeconfig)
  	return false;
  zfile_fseek (default_file, 0, SEEK_SET);
  while (zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), default_file)) {
  	if (tmp[0] && tmp[_tcslen (tmp) - 1] == '\n')
  		tmp[_tcslen (tmp) - 1] = 0;
  	if (!_tcscmp (tmp, s))
	    return true;
  }
  return false;
}

static size_t cfg_write (const void *b, struct zfile *z)
{
  size_t v;
  TCHAR lf = 10;
  v = zfile_fwrite (b, _tcslen ((TCHAR*)b), sizeof (TCHAR), z);
  zfile_fwrite (&lf, 1, 1, z);
  return v;
}

#define UTF8NAME _T(".utf8")

static void cfg_dowrite (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value, int d, int target)
{
  TCHAR tmp[CONFIG_BLEN], tmpext[CONFIG_BLEN];
	const TCHAR *optionp;

	if (value == NULL)
		return;
	if (optionext) {
		_tcscpy (tmpext, option);
		_tcscat (tmpext, optionext);
		optionp = tmpext;
	} else {
		optionp = option;
	}

  if (target)
  	_stprintf (tmp, _T("%s.%s=%s"), TARGET_NAME, optionp, value);
  else
  	_stprintf (tmp, _T("%s=%s"), optionp, value);
  if (d && isdefault (tmp))
  	return;
  cfg_write (tmp, f);
}

static void cfg_dowrite (struct zfile *f, const TCHAR *option, const TCHAR *value, int d, int target)
{
	cfg_dowrite (f, option, NULL, value, d, target);
}
void cfgfile_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 0, 0);
}
static void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 1, 0);
}
static void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, const TCHAR *optionext, bool b)
{
	cfg_dowrite (f, option, optionext, b ? _T("true") : _T("false"), 1, 0);
}
static void cfgfile_dwrite_bool (struct zfile *f, const TCHAR *option, int b)
{
	cfgfile_dwrite_bool (f, option, b != 0);
}
void cfgfile_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
  cfg_dowrite (f, option, value, 0, 0);
}
static void cfgfile_write_str (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value)
{
	cfg_dowrite (f, option, optionext, value, 0, 0);
}
void cfgfile_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
  cfg_dowrite (f, option, value, 1, 0);
}
static void cfgfile_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *optionext, const TCHAR *value)
{
	cfg_dowrite (f, option, optionext, value, 1, 0);
}

void cfgfile_target_write_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 0, 1);
}
void cfgfile_target_dwrite_bool (struct zfile *f, const TCHAR *option, bool b)
{
	cfg_dowrite (f, option, b ? _T("true") : _T("false"), 1, 1);
}
void cfgfile_target_write_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
  cfg_dowrite (f, option, value, 0, 1);
}
void cfgfile_target_dwrite_str (struct zfile *f, const TCHAR *option, const TCHAR *value)
{
  cfg_dowrite (f, option, value, 1, 1);
}

void cfgfile_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
  va_list parms;
  TCHAR tmp[CONFIG_BLEN];

  va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
  cfg_dowrite (f, option, tmp, 0, 0);
  va_end (parms);
}

void cfgfile_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
  va_list parms;
  TCHAR tmp[CONFIG_BLEN];

  va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
  cfg_dowrite (f, option, tmp, 1, 0);
  va_end (parms);
}
void cfgfile_target_write (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
  va_list parms;
  TCHAR tmp[CONFIG_BLEN];

  va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
  cfg_dowrite (f, option, tmp, 0, 1);
  va_end (parms);
}
void cfgfile_target_dwrite (struct zfile *f, const TCHAR *option, const TCHAR *format,...)
{
  va_list parms;
  TCHAR tmp[CONFIG_BLEN];

  va_start (parms, format);
	_vsntprintf (tmp, CONFIG_BLEN, format, parms);
  cfg_dowrite (f, option, tmp, 1, 1);
  va_end (parms);
}

static void cfgfile_write_rom (struct zfile *f, struct multipath *mp, const TCHAR *romfile, const TCHAR *name)
{
	TCHAR *str = cfgfile_subst_path (mp->path[0], UNEXPANDED, romfile);
	str = cfgfile_put_multipath (mp, str);
	cfgfile_write_str (f, name, str);
	struct zfile *zf = zfile_fopen (str, _T("rb"), ZFD_ALL);
	if (zf) {
		struct romdata *rd = getromdatabyzfile (zf);
		if (rd) {
			TCHAR name2[MAX_DPATH], str2[MAX_DPATH];
			_tcscpy (name2, name);
			_tcscat (name2, _T("_id"));
			_stprintf (str2, _T("%08X,%s"), rd->crc32, rd->name);
			cfgfile_write_str (f, name2, str2);
		}
		zfile_fclose (zf);
	}
	xfree (str);
}

static void cfgfile_to_path_save(const TCHAR *in, TCHAR *out, int type)
{
	if (_tcschr(in, '%')) {
		_tcscpy(out, in);
	} else {
		cfgfile_resolve_path_out_save(in, out, MAX_DPATH, type);
	}
}

static void cfgfile_write_path2(struct zfile *f, const TCHAR *option, const TCHAR *value, int type)
{
	if (_tcschr(value, '%')) {
		cfgfile_write_str(f, option, value);
	} else {
		TCHAR path[MAX_DPATH];
		cfgfile_resolve_path_out_save(value, path, MAX_DPATH, type);
		cfgfile_write_str(f, option, path);
	}
}

static void cfgfile_write_path (struct zfile *f, struct multipath *mp, const TCHAR *option, const TCHAR *value)
{
	TCHAR *s = cfgfile_put_multipath (mp, value);
	cfgfile_write_str (f, option, s);
	xfree (s);
}

static void cfgfile_adjust_path(TCHAR *path, int maxsz, struct multipath *mp)
{
	if (path[0] == 0)
		return;
	TCHAR *s = target_expand_environment(path, NULL, 0);
	_tcsncpy(path, s, maxsz - 1);
	path[maxsz - 1] = 0;
	if (mp) {
		for (int i = 0; i < MAX_PATHS; i++) {
			if (mp->path[i][0] && _tcscmp(mp->path[i], _T(".\\")) != 0 && _tcscmp(mp->path[i], _T("./")) != 0 && (path[0] != '/' && path[0] != '\\' && !_tcschr(path, ':'))) {
				TCHAR np[MAX_DPATH];
				_tcscpy(np, mp->path[i]);
				fixtrailing(np);
				_tcscat(np, s);
				if (zfile_exists(np)) {
					_tcsncpy(path, np, maxsz - 1);
					path[maxsz - 1] = 0;
					xfree(s);
					return;
				}
			}
		}
	}
	xfree(s);
}

static void cfgfile_resolve_path_out_all(const TCHAR *path, TCHAR *out, int size, int type, bool save)
{
	struct uae_prefs *p = &currprefs;
	TCHAR *s = NULL;
	switch (type)
	{
	case PATH_DIR:
		s = cfgfile_subst_path_load(UNEXPANDED, &p->path_hardfile, path, true);
		break;
	case PATH_HDF:
		s = cfgfile_subst_path_load(UNEXPANDED, &p->path_hardfile, path, false);
		break;
	case PATH_CD:
		s = cfgfile_subst_path_load(UNEXPANDED, &p->path_cd, path, false);
		break;
	case PATH_ROM:
		s = cfgfile_subst_path_load(UNEXPANDED, &p->path_rom, path, false);
		break;
	case PATH_FLOPPY:
		_tcscpy(out, path);
		cfgfile_adjust_path(out, MAX_DPATH, &p->path_floppy);
		break;
	default:
		s = cfgfile_subst_path(NULL, NULL, path);
		break;
	}
	if (s) {
		_tcscpy(out, s);
		xfree(s);
	}
}

void cfgfile_resolve_path_out_load(const TCHAR *path, TCHAR *out, int size, int type)
{
	cfgfile_resolve_path_out_all(path, out, size, type, false);
}
void cfgfile_resolve_path_load(TCHAR *path, int size, int type)
{
	cfgfile_resolve_path_out_all(path, path, size, type, false);
}
void cfgfile_resolve_path_out_save(const TCHAR *path, TCHAR *out, int size, int type)
{
	cfgfile_resolve_path_out_all(path, out, size, type, true);
}

static void write_filesys_config (struct uae_prefs *p, struct zfile *f)
{
	TCHAR tmp[MAX_DPATH], tmp2[MAX_DPATH], tmp3[MAX_DPATH], str1[MAX_DPATH], hdcs[MAX_DPATH];

  for (int i = 0; i < p->mountitems; i++) {
	  struct uaedev_config_data *uci = &p->mountconfig[i];
		struct uaedev_config_info *ci = &uci->ci;
		TCHAR *str1b, *str1c, *str2b;
		const TCHAR *str2;
    int bp = ci->bootpri;

		str2 = _T("");
		if (ci->rootdir[0] == ':') {
			TCHAR *ptr;
			// separate harddrive names
			_tcscpy(str1, ci->rootdir);
			ptr = _tcschr (str1 + 1, ':');
			if (ptr) {
				*ptr++ = 0;
				str2 = ptr;
				ptr = (TCHAR *) _tcschr (str2, ',');
				if (ptr)
					*ptr = 0;
			}
		} else {
			cfgfile_to_path_save(ci->rootdir, str1, ci->type == UAEDEV_DIR ? PATH_DIR : PATH_HDF);
    }
		int ct = ci->controller_type;
		int romtype = 0;
		if (ct >= HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST && ct <= HD_CONTROLLER_TYPE_IDE_LAST) {
			_stprintf(hdcs, _T("ide%d_%s"), ci->controller_unit, expansionroms[ct - HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST].name);
			romtype = expansionroms[ct - HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST].romtype;
		} else if (ct == HD_CONTROLLER_TYPE_IDE_AUTO) {
			_stprintf(hdcs, _T("ide%d"), ci->controller_unit);
		} else if (ct == HD_CONTROLLER_TYPE_UAE) {
			_stprintf(hdcs, _T("uae%d"), ci->controller_unit);
		}
		if (romtype) {
			for (int j = 0; hdcontrollers[j].label; j++) {
				if (hdcontrollers[j].romtype == (romtype & ROMTYPE_MASK)) {
					_stprintf(hdcs, hdcontrollers[j].label, ci->controller_unit);
					break;
				}
			}
		}
		if (ci->controller_type_unit > 0)
			_stprintf(hdcs + _tcslen(hdcs), _T("-%d"), ci->controller_type_unit + 1);

		str1b = cfgfile_escape (str1, _T(":,"), true);
		str1c = cfgfile_escape_min(str1);
		str2b = cfgfile_escape (str2, _T(":,"), true);
  	if (ci->type == UAEDEV_DIR) {
	    _stprintf (tmp, _T("%s,%s:%s:%s,%d"), ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), ci->volname, str1c, bp);
	    cfgfile_write_str (f, _T("filesystem2"), tmp);
			_tcscpy (tmp3, tmp);
	  } else  if (ci->type == UAEDEV_HDF) {
	    _stprintf (tmp, _T("%s,%s:%s,%d,%d,%d,%d,%d,%s,%s"),
		    ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), str1c,
				ci->sectors, ci->surfaces, ci->reserved, ci->blocksize,
				bp, _T(""), hdcs);
			_stprintf (tmp3, _T("%s,%s:%s%s%s,%d,%d,%d,%d,%d,%s,%s"),
				ci->readonly ? _T("ro") : _T("rw"),
				ci->devname ? ci->devname : _T(""), str1b, str2b[0] ? _T(":") : _T(""), str2b,
		    ci->sectors, ci->surfaces, ci->reserved, ci->blocksize,
				bp, _T(""), hdcs);
			if (ci->highcyl) {
				TCHAR *s = tmp + _tcslen (tmp);
				TCHAR *s2 = s;
				_stprintf (s2, _T(",%d"), ci->highcyl);
				_tcscat (tmp3, s2);
			}
			const TCHAR *extras = NULL;
      if (ct >= HD_CONTROLLER_TYPE_IDE_FIRST && ct <= HD_CONTROLLER_TYPE_IDE_LAST) {
				if (ci->unit_feature_level == HD_LEVEL_ATA_1) {
					extras = _T("ATA1");
				} else if (ci->unit_feature_level == HD_LEVEL_ATA_2S) {
					extras = _T("ATA2+S");
				}
			}
			if (extras) {
				_tcscat(tmp, _T(","));
				_tcscat(tmp3, _T(","));
				_tcscat(tmp, extras);
				_tcscat(tmp3, extras);
			}
			if (ci->unit_special_flags) {
				TCHAR tmpx[32];
				_stprintf(tmpx, _T(",flags=0x%x"), ci->unit_special_flags);
				_tcscat(tmp, tmpx);
				_tcscat(tmp3, tmpx);
			}
			if (ci->lock) {
				_tcscat(tmp, _T(",lock"));
				_tcscat(tmp3, _T(",lock"));
			}

			if (ci->type == UAEDEV_HDF)
	      cfgfile_write_str (f, _T("hardfile2"), tmp);
	  }
	  _stprintf (tmp2, _T("uaehf%d"), i);
		cfgfile_write (f, tmp2, _T("%s,%s"), ci->type == UAEDEV_HDF ? _T("hdf") : _T("dir"), tmp3);
		xfree (str1b);
		xfree (str1c);
		xfree (str2b);
  }
}

static void write_compatibility_cpu(struct zfile *f, struct uae_prefs *p)
{
  TCHAR tmp[100];
  int model;

  model = p->cpu_model;
  if (model == 68030)
  	model = 68020;
  if (p->address_space_24 && model == 68020)
  	_tcscpy (tmp, _T("68ec020"));
  else
	  _stprintf(tmp, _T("%d"), model);
  if (model == 68020 && (p->fpu_model == 68881 || p->fpu_model == 68882)) 
  	_tcscat(tmp, _T("/68881"));
  cfgfile_write (f, _T("cpu_type"), tmp);
}

static void write_leds (struct zfile *f, const TCHAR *name, int mask)
{
	TCHAR tmp[MAX_DPATH];
	tmp[0] = 0;
	for (int i = 0; leds[i]; i++) {
		bool got = false;
		for (int j = 0; leds[j]; j++) {
			if (leds_order[j] == i) {
				if (mask & (1 << j)) {
					if (got)
						_tcscat (tmp, _T(":"));
					_tcscat (tmp, leds[j]);
					got = true;
				}
			}
		}
		if (leds[i + 1] && got)
			_tcscat (tmp, _T(","));
	}
	while (tmp[0] && tmp[_tcslen (tmp) - 1] == ',')
		tmp[_tcslen (tmp) - 1] = 0;
	cfgfile_dwrite_str (f, name, tmp);
}

static void write_resolution (struct zfile *f, const TCHAR *ws, const TCHAR *hs, struct wh *wh)
{
	cfgfile_write (f, ws, _T("%d"), wh->width);
	cfgfile_write (f, hs, _T("%d"), wh->height);
}

static void cfgfile_write_board_rom(struct uae_prefs *prefs, struct zfile *f, struct multipath *mp, struct boardromconfig *br)
{
	TCHAR buf[MAX_DPATH];
	TCHAR name[256];
	const struct expansionromtype *ert;
	
	if (br->device_type == 0)
		return;
	ert = get_device_expansion_rom(br->device_type);
	if (!ert)
		return;
	for (int i = 0; i < MAX_BOARD_ROMS; i++) {
		struct romconfig *rc = &br->roms[i];
		if (br->device_num == 0)
			_tcscpy(name, ert->name);
		else
			_stprintf(name, _T("%s-%d"), ert->name, br->device_num + 1);
		if (i == 0 || _tcslen(br->roms[i].romfile)) {
			_stprintf(buf, _T("%s%s_rom_file"), name, i ? _T("_ext") : _T(""));
			cfgfile_write_rom (f, mp, br->roms[i].romfile, buf);
			if (rc->romident[0]) {
				_stprintf(buf, _T("%s%s_rom"), name, i ? _T("_ext") : _T(""));
				cfgfile_dwrite_str (f, buf, rc->romident);
			}
			if (rc->inserted || br->device_order > 0) {
				TCHAR buf2[256], *p;
				buf2[0] = 0;
				p = buf2;
				_stprintf(buf, _T("%s%s_rom_options"), name, i ? _T("_ext") : _T(""));
				if (rc->inserted) {
					if (buf2[0])
						_tcscat(buf2, _T(","));
					_tcscat(buf2, _T("inserted=true"));
				}
				if (buf2[0])
					cfgfile_dwrite_str (f, buf, buf2);
			}

			if (rc->board_ram_size) {
				_stprintf(buf, _T("%s%s_mem_size"), name, i ? _T("_ext") : _T(""));
				cfgfile_write(f, buf, _T("%d"), rc->board_ram_size / 0x40000);
			}
		}
	}
}

static bool cfgfile_readramboard(const TCHAR *option, const TCHAR *value, const TCHAR *name, struct ramboard *rbp)
{
	TCHAR tmp1[MAX_DPATH];
	int v;
	struct ramboard *rb = &rbp[0];
	_stprintf(tmp1, _T("%s_size"), name);
	if (!_tcsicmp(option, tmp1)) {
		v = 0;
		if (!_tcsicmp(tmp1, _T("chipmem_size")))
			return false;
		cfgfile_intval(option, value, tmp1, &v, 0x100000);
		rb->size = v;
		return true;
	}
		_stprintf(tmp1, _T("%s_size_k"), name);
	if (!_tcsicmp(option, tmp1)) {
		v = 0;
		cfgfile_intval(option, value, tmp1, &v, 1024);
		rb->size = v;
		return true;
	}
  return false;
}

static void cfgfile_save_options (struct zfile *f, struct uae_prefs *p, int type)
{
  struct strlist *sl;
  TCHAR tmp[MAX_DPATH];
  int i;

  cfgfile_write_str (f, _T("config_description"), p->description);
  cfgfile_write_bool (f, _T("config_hardware"), type & CONFIG_TYPE_HARDWARE);
  cfgfile_write_bool (f, _T("config_host"), !!(type & CONFIG_TYPE_HOST));
  if (p->info[0])
  	cfgfile_write (f, _T("config_info"), p->info);
  cfgfile_write (f, _T("config_version"), _T("%d.%d.%d"), UAEMAJOR, UAEMINOR, UAESUBREV);
   
  for (sl = p->all_lines; sl; sl = sl->next) {
  	if (sl->unknown) {
			if (sl->option)
	      cfgfile_write_str (f, sl->option, sl->value);
    }
  }

	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_rom.path[i][0]) {
      _stprintf (tmp, _T("%s.rom_path"), TARGET_NAME);
      cfgfile_write_str (f, tmp, p->path_rom.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_floppy.path[i][0]) {
      _stprintf (tmp, _T("%s.floppy_path"), TARGET_NAME);
      cfgfile_write_str (f, tmp, p->path_floppy.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_hardfile.path[i][0]) {
      _stprintf (tmp, _T("%s.hardfile_path"), TARGET_NAME);
      cfgfile_write_str (f, tmp, p->path_hardfile.path[i]);
		}
	}
	for (i = 0; i < MAX_PATHS; i++) {
		if (p->path_cd.path[i][0]) {
	    _stprintf (tmp, _T("%s.cd_path"), TARGET_NAME);
	    cfgfile_write_str (f, tmp, p->path_cd.path[i]);
    }
  }

  cfg_write (_T("; host-specific"), f);

  target_save_options (f, p);

  cfg_write (_T("; common"), f);

  cfgfile_write_str (f, _T("use_gui"), guimode1[p->start_gui]);

	cfgfile_write_rom (f, &p->path_rom, p->romfile, _T("kickstart_rom_file"));
  cfgfile_write_rom (f, &p->path_rom, p->romextfile, _T("kickstart_ext_rom_file"));
	if (p->romident[0])
		cfgfile_dwrite_str (f, _T("kickstart_rom"), p->romident);
	if (p->romextident[0])
		cfgfile_write_str (f, _T("kickstart_ext_rom="), p->romextident);

	for (int i = 0; i < MAX_EXPANSION_BOARDS; i++) {
		cfgfile_write_board_rom(p, f, &p->path_rom, &p->expansionboard[i]);
	}

	cfgfile_write_path2(f, _T("flash_file"), p->flashfile, PATH_ROM);
	cfgfile_write_path (f, &p->path_rom, _T("cart_file"), p->cartfile);
	if (p->cartident[0])
		cfgfile_write_str (f, _T("cart"), p->cartident);

	cfgfile_write (f, _T("floppy_volume"), _T("%d"), p->dfxclickvolume_disk[0]);
  p->nr_floppies = 4;
  for (i = 0; i < 4; i++) {
    _stprintf (tmp, _T("floppy%d"), i);
		cfgfile_write_path2(f, tmp, p->floppyslots[i].df, PATH_FLOPPY);
		_stprintf (tmp, _T("floppy%dwp"), i);
		cfgfile_dwrite_bool (f, tmp, p->floppyslots[i].forcedwriteprotect);
  	_stprintf (tmp, _T("floppy%dtype"), i);
  	cfgfile_dwrite (f, tmp, _T("%d"), p->floppyslots[i].dfxtype);
		_stprintf (tmp, _T("floppy%dsound"), i);
		cfgfile_dwrite (f, tmp, _T("%d"), p->floppyslots[i].dfxclick);
		if (p->floppyslots[i].dfxclick) {
			_stprintf (tmp, _T("floppy%dsoundvolume_disk"), i);
			cfgfile_write (f, tmp, _T("%d"), p->dfxclickvolume_disk[i]);
			_stprintf (tmp, _T("floppy%dsoundvolume_empty"), i);
			cfgfile_write (f, tmp, _T("%d"), p->dfxclickvolume_empty[i]);
		}
  	if (p->floppyslots[i].dfxtype < 0 && p->nr_floppies > i)
	    p->nr_floppies = i;
  }

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		if (p->cdslots[i].name[0] || p->cdslots[i].inuse) {
			TCHAR tmp2[MAX_DPATH];
			_stprintf (tmp, _T("cdimage%d"), i);
			cfgfile_to_path_save(p->cdslots[i].name, tmp2, PATH_CD);
			if (p->cdslots[i].type != SCSI_UNIT_DEFAULT || _tcschr (p->cdslots[i].name, ',') || p->cdslots[i].delayed) {
				_tcscat (tmp2, _T(","));
				if (p->cdslots[i].delayed) {
					_tcscat (tmp2, _T("delay"));
					_tcscat (tmp2, _T(":"));
				}
				if (p->cdslots[i].type != SCSI_UNIT_DEFAULT) {
					_tcscat (tmp2, cdmodes[p->cdslots[i].type + 1]);
				}
			}
			cfgfile_write_str (f, tmp, tmp2);
		}
	}

  cfgfile_write (f, _T("nr_floppies"), _T("%d"), p->nr_floppies);
	cfgfile_dwrite_bool (f, _T("floppy_write_protect"), p->floppy_read_only);
  cfgfile_write (f, _T("floppy_speed"), _T("%d"), p->floppy_speed);
	cfgfile_write (f, _T("cd_speed"), _T("%d"), p->cd_speed);
	cfgfile_write_str (f, _T("scsi"), scsimode[p->scsi]);

  cfgfile_write_str (f, _T("sound_output"), soundmode1[p->produce_sound]);
  cfgfile_write_str (f, _T("sound_channels"), stereomode[p->sound_stereo]);
  cfgfile_write (f, _T("sound_stereo_separation"), _T("%d"), p->sound_stereo_separation);
  cfgfile_write (f, _T("sound_stereo_mixing_delay"), _T("%d"), p->sound_mixed_stereo_delay >= 0 ? p->sound_mixed_stereo_delay : 0);
  cfgfile_write (f, _T("sound_frequency"), _T("%d"), p->sound_freq);
  cfgfile_write_str (f, _T("sound_interpol"), interpolmode[p->sound_interpol]);
  cfgfile_write_str (f, _T("sound_filter"), soundfiltermode1[p->sound_filter]);
  cfgfile_write_str (f, _T("sound_filter_type"), soundfiltermode2[p->sound_filter_type]);
	cfgfile_write (f, _T("sound_volume_paula"), _T("%d"), p->sound_volume_paula);
	if (p->sound_volume_cd >= 0)
		cfgfile_write (f, _T("sound_volume_cd"), _T("%d"), p->sound_volume_cd);

#ifdef USE_JIT_FPU
	cfgfile_write_bool (f, _T("compfpu"), p->compfpu);
#endif
  cfgfile_write (f, _T("cachesize"), _T("%d"), p->cachesize);

	for (i = 0; i < MAX_JPORTS; i++) {
		struct jport *jp = &p->jports[i];
		int v = jp->id;
		TCHAR tmp1[MAX_DPATH], tmp2[MAX_DPATH];
		if (v == JPORT_NONE) {
			_tcscpy (tmp2, _T("none"));
		} else if (v < JSEM_JOYS) {
			_stprintf(tmp2, _T("kbd%d"), v + 1);
		} else if (v < JSEM_MICE) {
			_stprintf (tmp2, _T("joy%d"), v - JSEM_JOYS);
		} else {
			_tcscpy (tmp2, _T("mouse"));
			if (v - JSEM_MICE > 0)
				_stprintf (tmp2, _T("mouse%d"), v - JSEM_MICE);
		}
		if (i < 2 || jp->id >= 0) {
			_stprintf (tmp1, _T("joyport%d"), i);
			cfgfile_write (f, tmp1, tmp2);
			_stprintf (tmp1, _T("joyport%dautofire"), i);
			cfgfile_write (f, tmp1, joyaf[jp->autofire]);
			if (i < 2 && jp->mode > 0) {
				_stprintf (tmp1, _T("joyport%dmode"), i);
				cfgfile_write (f, tmp1, joyportmodes[jp->mode]);
			}
			if (jp->idc.name[0]) {
				_stprintf (tmp1, _T("joyportfriendlyname%d"), i);
				cfgfile_write (f, tmp1, jp->idc.name);
			}
			if (jp->idc.configname[0]) {
				_stprintf (tmp1, _T("joyportname%d"), i);
				cfgfile_write (f, tmp1, jp->idc.configname);
			}
			if (jp->nokeyboardoverride) {
				_stprintf (tmp1, _T("joyport%dkeyboardoverride"), i);
				cfgfile_write_bool (f, tmp1, !jp->nokeyboardoverride);
      }
		}
	}

	cfgfile_write_bool (f, _T("bsdsocket_emu"), p->socket_emu);

	cfgfile_dwrite_str (f, _T("boot_rom_uae"), uaebootrom[p->boot_rom]);
	cfgfile_dwrite_str(f, _T("uaeboard"), uaeboard[p->uaeboard]);
  cfgfile_dwrite_str (f, _T("absolute_mouse"), abspointers[p->input_tablet]);

  cfgfile_write (f, _T("gfx_framerate"), _T("%d"), p->gfx_framerate);
  write_resolution (f, _T("gfx_width"), _T("gfx_height"), &p->gfx_monitor.gfx_size); /* compatibility with old versions */
	cfgfile_write (f, _T("gfx_top_windowed"), _T("%d"), p->gfx_monitor.gfx_size.y);
	cfgfile_write(f, _T("gfx_left_windowed"), _T("%d"), p->gfx_monitor.gfx_size.x);
	cfgfile_write (f, _T("gfx_refreshrate"), _T("%d"), p->gfx_apmode[0].gfx_refreshrate);
	cfgfile_dwrite (f, _T("gfx_refreshrate_rtg"), _T("%d"), p->gfx_apmode[1].gfx_refreshrate);

  cfgfile_write_bool (f, _T("gfx_lores"), p->gfx_resolution == 0);
  cfgfile_write_str (f, _T("gfx_resolution"), lorestype1[p->gfx_resolution]);
	cfgfile_write_str (f, _T("gfx_linemode"), p->gfx_vresolution > 0 ? linemode[1] : linemode[0]);

  cfgfile_write_bool (f, _T("immediate_blits"), p->immediate_blits);
	cfgfile_dwrite_str (f, _T("waiting_blits"), waitblits[p->waiting_blits]);
  cfgfile_write_bool (f, _T("fast_copper"), p->fast_copper);
  cfgfile_write_bool (f, _T("ntsc"), p->ntscmode);

	cfgfile_dwrite_bool (f, _T("show_leds"), !!(p->leds_on_screen & STATUSLINE_CHIPSET));
	cfgfile_dwrite_bool (f, _T("show_leds_rtg"), !!(p->leds_on_screen & STATUSLINE_RTG));
	write_leds(f, _T("show_leds_enabled"), p->leds_on_screen_mask[0]);
	write_leds(f, _T("show_leds_enabled_rtg"), p->leds_on_screen_mask[1]);

  if (p->chipset_mask & CSMASK_AGA)
  	cfgfile_write (f, _T("chipset"), _T("aga"));
  else if ((p->chipset_mask & CSMASK_ECS_AGNUS) && (p->chipset_mask & CSMASK_ECS_DENISE))
  	cfgfile_write (f, _T("chipset"), _T("ecs"));
  else if (p->chipset_mask & CSMASK_ECS_AGNUS)
  	cfgfile_write (f, _T("chipset"), _T("ecs_agnus"));
  else if (p->chipset_mask & CSMASK_ECS_DENISE)
  	cfgfile_write (f, _T("chipset"), _T("ecs_denise"));
  else
  	cfgfile_write (f, _T("chipset"), _T("ocs"));
	if (p->chipset_refreshrate > 0)
		cfgfile_write (f, _T("chipset_refreshrate"), _T("%f"), p->chipset_refreshrate);

  cfgfile_write_str (f, _T("collision_level"), collmode[p->collision_level]);

	cfgfile_write_str(f, _T("chipset_compatible"), cscompa[p->cs_compatible]);
	cfgfile_dwrite_str(f, _T("ciaatod"), ciaatodmode[p->cs_ciaatod]);
	cfgfile_dwrite_str(f, _T("rtc"), rtctype[p->cs_rtc]);
	cfgfile_dwrite_bool(f, _T("ksmirror_e0"), p->cs_ksmirror_e0);
	cfgfile_dwrite_bool(f, _T("ksmirror_a8"), p->cs_ksmirror_a8);
	cfgfile_dwrite_bool (f, _T("cd32cd"), p->cs_cd32cd);
	cfgfile_dwrite_bool (f, _T("cd32c2p"), p->cs_cd32c2p);
	cfgfile_dwrite_bool (f, _T("cd32nvram"), p->cs_cd32nvram);
	cfgfile_dwrite (f, _T("cd32nvram_size"), _T("%d"), p->cs_cd32nvram_size / 1024);
	cfgfile_dwrite(f, _T("fatgary"), _T("%d"), p->cs_fatgaryrev);
	cfgfile_dwrite(f, _T("ramsey"), _T("%d"), p->cs_ramseyrev);
	cfgfile_dwrite_bool(f, _T("pcmcia"), p->cs_pcmcia);
	cfgfile_dwrite_bool(f, _T("cia_todbug"), p->cs_ciatodbug);
	cfgfile_dwrite_bool(f, _T("z3_autoconfig"), p->cs_z3autoconfig);
	cfgfile_dwrite_str(f, _T("ciaa_type"), ciatype[p->cs_ciatype[0]]);
	cfgfile_dwrite_str(f, _T("ciab_type"), ciatype[p->cs_ciatype[1]]);
	cfgfile_dwrite_str(f, _T("unmapped_address_space"), unmapped[p->cs_unmapped_space]);

	if (is_board_enabled(p, ROMTYPE_CD32CART, 0)) {
		cfgfile_dwrite_bool(f, _T("cd32fmv"), true);
	}
	if (is_board_enabled(p, ROMTYPE_MB_IDE, 0) && p->cs_ide == 1) {
		cfgfile_dwrite_str(f, _T("ide"), _T("a600/a1200"));
	}
	if (is_board_enabled(p, ROMTYPE_MB_IDE, 0) && p->cs_ide == 2) {
		cfgfile_dwrite_str(f, _T("ide"), _T("a4000"));
	}

	cfgfile_dwrite_str (f, _T("z3mapping"), z3mapping[p->z3_mapping_mode]);
  if (p->fastmem[0].size < 0x100000 && p->fastmem[0].size) {
		_tcscpy(tmp, _T("fastmem_size_k"));
	  cfgfile_write (f, tmp, _T("%d"), p->fastmem[0].size / 1024);
  } else {
		_tcscpy(tmp, _T("fastmem_size"));
		cfgfile_write(f, tmp, _T("%d"), p->fastmem[0].size / 0x100000);
  }
	cfgfile_write (f, _T("a3000mem_size"), _T("%d"), p->mbresmem_low.size / 0x100000);
	cfgfile_write (f, _T("mbresmem_size"), _T("%d"), p->mbresmem_high.size / 0x100000);
	_tcscpy(tmp, _T("z3mem_size"));
	cfgfile_write(f, tmp, _T("%d"), p->z3fastmem[0].size / 0x100000);
  cfgfile_write (f, _T("z3mem_start"), _T("0x%x"), p->z3autoconfig_start);
  cfgfile_write (f, _T("bogomem_size"), _T("%d"), p->bogomem.size / 0x40000);
	struct rtgboardconfig *rbc = &p->rtgboards[0];
	if (rbc->rtgmem_size) {
		_tcscpy(tmp, _T("gfxcard_size"));
		cfgfile_write(f, tmp, _T("%d"), rbc->rtgmem_size / 0x100000);
		_tcscpy(tmp, _T("gfxcard_type"));
		cfgfile_dwrite_str(f, tmp, gfxboard_get_configname(rbc->rtgmem_type));
	}
  cfgfile_write (f, _T("chipmem_size"), _T("%d"), p->chipmem.size == 0x20000 ? -1 : (p->chipmem.size == 0x40000 ? 0 : p->chipmem.size / 0x80000));
	// do not save aros rom special space
	if (!(p->custom_memory_sizes[0] == 512 * 1024 && p->custom_memory_sizes[1] == 512 * 1024 && p->custom_memory_addrs[0] == 0xa80000 && p->custom_memory_addrs[1] == 0xb00000)) {
		if (p->custom_memory_sizes[0])
			cfgfile_write (f, _T("addmem1"), _T("0x%x,0x%x"), p->custom_memory_addrs[0], p->custom_memory_sizes[0]);
		if (p->custom_memory_sizes[1])
			cfgfile_write (f, _T("addmem2"), _T("0x%x,0x%x"), p->custom_memory_addrs[1], p->custom_memory_sizes[1]);
	}

  if (p->m68k_speed > 0) {
  	cfgfile_write (f, _T("finegrain_cpu_speed"), _T("%d"), p->m68k_speed);
	} else {
  	cfgfile_write_str (f, _T("cpu_speed"), p->m68k_speed < 0 ? _T("max") : _T("real"));
  }

  /* do not reorder start */
  write_compatibility_cpu(f, p);
  cfgfile_write (f, _T("cpu_model"), _T("%d"), p->cpu_model);
  if (p->fpu_model)
  	cfgfile_write (f, _T("fpu_model"), _T("%d"), p->fpu_model);
  cfgfile_write_bool (f, _T("cpu_compatible"), p->cpu_compatible);
  cfgfile_write_bool (f, _T("cpu_24bit_addressing"), p->address_space_24);
  /* do not reorder end */

	cfgfile_write_bool (f, _T("cpu_cycle_exact"), p->cpu_cycle_exact);
	// must be after cpu_cycle_exact
	cfgfile_write_bool (f, _T("cpu_memory_cycle_exact"), p->cpu_memory_cycle_exact);
	cfgfile_write_bool (f, _T("blitter_cycle_exact"), p->blitter_cycle_exact);
	// must be after cpu_cycle_exact, cpu_memory_cycle_exact and blitter_cycle_exact
	if (p->cpu_cycle_exact && p->blitter_cycle_exact)
		cfgfile_write_str (f, _T("cycle_exact"), cycleexact[2]);
	else if (p->cpu_memory_cycle_exact && p->blitter_cycle_exact)
		cfgfile_write_str (f, _T("cycle_exact"), cycleexact[1]);
	else
		cfgfile_write_str (f, _T("cycle_exact"), cycleexact[0]);

	cfgfile_dwrite_bool (f, _T("fpu_no_unimplemented"), p->fpu_no_unimplemented);
	cfgfile_write_bool (f, _T("fpu_strict"), p->fpu_strict);

  cfgfile_write (f, _T("rtg_modes"), _T("0x%x"), p->picasso96_modeflags);

	cfgfile_write_str (f, _T("kbd_lang"), (p->keyboard_lang == KBD_LANG_DE ? _T("de")
		: p->keyboard_lang == KBD_LANG_DK ? _T("dk")
		: p->keyboard_lang == KBD_LANG_ES ? _T("es")
		: p->keyboard_lang == KBD_LANG_US ? _T("us")
		: p->keyboard_lang == KBD_LANG_SE ? _T("se")
		: p->keyboard_lang == KBD_LANG_FR ? _T("fr")
		: p->keyboard_lang == KBD_LANG_IT ? _T("it")
		: _T("FOO")));

	cfgfile_dwrite_bool (f, _T("warp"), p->turbo_emulation);

#ifdef FILESYS
	write_filesys_config (p, f);
	cfgfile_dwrite (f, _T("filesys_max_size"), _T("%d"), p->filesys_limit);
	cfgfile_dwrite (f, _T("filesys_max_name_length"), _T("%d"), p->filesys_max_name);
#endif
	cfgfile_dwrite_bool(f, _T("harddrive_write_protect"), p->harddrive_read_only);

  write_inputdevice_config (p, f);
}

static int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, bool numbercheck)
{
  if (name != NULL && _tcscmp (option, name) != 0)
  	return 0;
	if (_tcsicmp (value, _T("yes")) == 0 || _tcsicmp (value, _T("y")) == 0
		|| _tcsicmp (value, _T("true")) == 0 || _tcsicmp (value, _T("t")) == 0
		|| (numbercheck && _tcsicmp (value, _T("1")) == 0))
  	*location = 1;
	else if (_tcsicmp (value, _T("no")) == 0 || _tcsicmp (value, _T("n")) == 0
		|| _tcsicmp (value, _T("false")) == 0 || _tcsicmp (value, _T("f")) == 0
		|| (numbercheck && _tcsicmp (value, _T("0")) == 0))
	  *location = 0;
  else {
		cfgfile_warning(_T("Option '%s' requires a value of either 'true' or 'false' (was '%s').\n"), option, value);
	  return -1;
  }
  return 1;
}

static int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location)
{
	return cfgfile_yesno (option, value, name, location, true);
}

static int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location, bool numbercheck)
{
	int val;
	int ret = cfgfile_yesno (option, value, name, &val, numbercheck);
	if (ret == 0)
		return 0;
	if (ret < 0)
		*location = false;
	else
		*location = val != 0;
	return 1;
}

int cfgfile_yesno (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location)
{
	return cfgfile_yesno (option, value, name, location, true);
}

static int cfgfile_doubleval (const TCHAR *option, const TCHAR *value, const TCHAR *name, double *location)
{
	TCHAR *endptr;
	if (name != NULL && _tcscmp (option, name) != 0)
		return 0;
	*location = _tcstod (value, &endptr);
	return 1;
}

static int cfgfile_floatval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, float *location)
{
	TCHAR *endptr;
	if (name == NULL)
		return 0;
	if (nameext) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	*location = (float)_tcstod (value, &endptr);
	return 1;
}

static int cfgfile_floatval (const TCHAR *option, const TCHAR *value, const TCHAR *name, float *location)
{
	return cfgfile_floatval (option, value, name, NULL, location);
}

static int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, unsigned int *location, int scale)
{
  int base = 10;
  TCHAR *endptr;
	TCHAR tmp[MAX_DPATH];

	if (name == NULL)
  	return 0;
	if (nameext) {
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
  /* I guess octal isn't popular enough to worry about here...  */
  if (value[0] == '0' && _totupper(value[1]) == 'X')
  	value += 2, base = 16;
  *location = _tcstol (value, &endptr, base) * scale;

  if (*endptr != '\0' || *value == '\0') {
		if (_tcsicmp (value, _T("false")) == 0 || _tcsicmp (value, _T("no")) == 0) {
			*location = 0;
			return 1;
		}
		if (_tcsicmp (value, _T("true")) == 0 || _tcsicmp (value, _T("yes")) == 0) {
			*location = 1;
			return 1;
  	}
		cfgfile_warning(_T("Option '%s' requires a numeric argument but got '%s'\n"), nameext ? tmp : option, value);
  	return -1;
  }
  return 1;
}
static int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, unsigned int *location, int scale)
{
	return cfgfile_intval (option, value, name, NULL, location, scale);
}
int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, int scale)
{
	unsigned int v = 0;
	int r = cfgfile_intval (option, value, name, NULL, &v, scale);
	if (!r)
		return 0;
	*location = (int)v;
	return r;
}
static int cfgfile_intval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, int *location, int scale)
{
	unsigned int v = 0;
	int r = cfgfile_intval (option, value, name, nameext, &v, scale);
	if (!r)
		return 0;
	*location = (int)v;
	return r;
}

static int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, int *location, const TCHAR *table[], int more)
{
  int val;
	TCHAR tmp[MAX_DPATH];
	if (name == NULL)
  	return 0;
	if (nameext) {
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
  val = match_string (table, value);
  if (val == -1) {
  	if (more)
	    return 0;
		if (!_tcsicmp (value, _T("yes")) || !_tcsicmp (value, _T("true"))) {
			val = 1;
		} else if  (!_tcsicmp (value, _T("no")) || !_tcsicmp (value, _T("false"))) {
			val = 0;
		} else {
			cfgfile_warning(_T("Unknown value ('%s') for option '%s'.\n"), value, nameext ? tmp : option);
    	return -1;
		}
  }
  *location = val;
  return 1;
}
int cfgfile_strval (const TCHAR *option, const TCHAR *value, const TCHAR *name, int *location, const TCHAR *table[], int more)
{
	return cfgfile_strval (option, value, name, NULL, location, table, more);
}

static int cfgfile_strboolval (const TCHAR *option, const TCHAR *value, const TCHAR *name, bool *location, const TCHAR *table[], int more)
{
	int locationint;
	if (!cfgfile_strval (option, value, name, &locationint, table, more))
		return 0;
	*location = locationint != 0;
	return 1;
}

int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
  if (_tcscmp (option, name) != 0)
  	return 0;
  _tcsncpy (location, value, maxsz - 1);
  location[maxsz - 1] = '\0';
  return 1;
}

static int cfgfile_string (const TCHAR *option, const TCHAR *value, const TCHAR *name, const TCHAR *nameext, TCHAR *location, int maxsz)
{
	if (nameext) {
		TCHAR tmp[MAX_DPATH];
		_tcscpy (tmp, name);
		_tcscat (tmp, nameext);
		if (_tcscmp (tmp, option) != 0)
			return 0;
	} else {
		if (_tcscmp (option, name) != 0)
			return 0;
	}
	_tcsncpy (location, value, maxsz - 1);
	location[maxsz - 1] = '\0';
	return 1;
}

static int cfgfile_path (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz, struct multipath *mp)
{
	if (!cfgfile_string (option, value, name, location, maxsz))
		return 0;
	cfgfile_adjust_path(location, maxsz, mp);
	return 1;
}

static int cfgfile_path (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	return cfgfile_path (option, value, name, location, maxsz, NULL);
}

static int cfgfile_multipath (const TCHAR *option, const TCHAR *value, const TCHAR *name, struct multipath *mp, struct uae_prefs *p)
{
	TCHAR tmploc[MAX_DPATH];
	if (!cfgfile_string (option, value, name, tmploc, MAX_DPATH))
		return 0;
	for (int i = 0; i < MAX_PATHS; i++) {
		if (mp->path[i][0] == 0 || (i == 0 && (!_tcscmp (mp->path[i], _T(".\\")) || !_tcscmp (mp->path[i], _T("./"))))) {
			TCHAR *s = target_expand_environment (tmploc, NULL, 0);
			_tcsncpy (mp->path[i], s, PATH_MAX - 1);
			mp->path[i][PATH_MAX - 1] = 0;
			fixtrailing (mp->path[i]);
			xfree (s);
			return 1;
		}
	}
	return 1;
}

static int cfgfile_rom (const TCHAR *option, const TCHAR *value, const TCHAR *name, TCHAR *location, int maxsz)
{
	TCHAR id[MAX_DPATH];
	if (!cfgfile_string (option, value, name, id, sizeof id / sizeof (TCHAR)))
		return 0;
	TCHAR *p = _tcschr (id, ',');
	if (p) {
		TCHAR *endptr, tmp;
		*p = 0;
		tmp = id[4];
		id[4] = 0;
		uae_u32 crc32 = _tcstol (id, &endptr, 16) << 16;
		id[4] = tmp;
		crc32 |= _tcstol (id + 4, &endptr, 16);
		struct romdata *rd = getromdatabycrc (crc32, true);
		if (rd) {
			struct romdata *rd2 = getromdatabyid (rd->id);
			if (rd->group == 0 && rd2 == rd) {
				if (zfile_exists (location))
					return 1;
			}
			if (rd->group && rd2)
				rd = rd2;
			struct romlist *rl = getromlistbyromdata (rd);
			if (rl) {
				write_log (_T("%s: %s -> %s\n"), name, location, rl->path);
				_tcsncpy (location, rl->path, maxsz);
			}
		}
	}
	return 1;
}

static int getintval (TCHAR **p, int *result, int delim)
{
  TCHAR *value = *p;
  int base = 10;
  TCHAR *endptr;
  TCHAR *p2 = _tcschr (*p, delim);

  if (p2 == 0)
  	return 0;

  *p2++ = '\0';

  if (value[0] == '0' && _totupper (value[1]) == 'X')
  	value += 2, base = 16;
  *result = _tcstol (value, &endptr, base);
  *p = p2;

  if (*endptr != '\0' || *value == '\0')
  	return 0;

  return 1;
}

static int getintval2 (TCHAR **p, int *result, int delim, bool last)
{
  TCHAR *value = *p;
  int base = 10;
  TCHAR *endptr;
	TCHAR *p2;

	p2 = _tcschr (*p, delim);
  if (p2 == 0) {
		if (last) {
			if (delim != '.')
				p2 = _tcschr (*p, ',');
  	  if (p2 == 0) {
				p2 = *p;
				while(*p2)
					p2++;
				if (p2 == *p)
	        return 0;
    	}
		} else {
			return 0;
    }
	}
	if (!_istdigit(**p) && **p != '-' && **p != '+')
		return 0;

  if (*p2 != 0)
  	*p2++ = '\0';

  if (value[0] == '0' && _totupper (value[1]) == 'X')
  	value += 2, base = 16;
  *result = _tcstol (value, &endptr, base);
  *p = p2;

  if (*endptr != '\0' || *value == '\0') {
	  *p = 0;
	  return 0;
  }

  return 1;
}

static int cfgfile_option_bool(TCHAR *s, const TCHAR *option)
{
	TCHAR buf[MAX_DPATH];
	if (!s)
		return -1;
	_tcscpy(buf, s);
	_tcscat(buf, _T(","));
	TCHAR *p = buf;
	for (;;) {
		TCHAR *tmpp = _tcschr (p, ',');
		if (tmpp == NULL)
			return -1;
		*tmpp++ = 0;
		TCHAR *tmpp2 = _tcschr(p, '=');
		if (tmpp2)
			*tmpp2++ = 0;
		if (!_tcsicmp(p, option)) {
			if (!tmpp2)
				return 0;
			TCHAR *tmpp3 = _tcschr (tmpp2, ',');
			if (tmpp3)
				*tmpp3 = 0;
			if (tmpp2 && !_tcsicmp(tmpp2, _T("true")))
				return 1;
			if (tmpp2 && !_tcsicmp(tmpp2, _T("false")))
				return 0;
			return 1;
		}
		p = tmpp;
	}
}
static void set_chipset_mask (struct uae_prefs *p, int val)
{
  p->chipset_mask = (val == 0 ? 0
    : val == 1 ? CSMASK_ECS_AGNUS
    : val == 2 ? CSMASK_ECS_DENISE
    : val == 3 ? CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS
    : CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS);
}

static int cfgfile_parse_host (struct uae_prefs *p, TCHAR *option, TCHAR *value)
{
	int i, v;
  bool vb;
  TCHAR *section = 0;
  TCHAR *tmpp;
  TCHAR tmpbuf[CONFIG_BLEN];

	if (_tcsncmp (option, _T("input."), 6) == 0 || _tcsncmp(option, _T("input_"), 6) == 0) {
    read_inputdevice_config (p, option, value);
    return 1;
  }

  for (tmpp = option; *tmpp != '\0'; tmpp++)
  	if (_istupper (*tmpp))
	    *tmpp = _totlower (*tmpp);
  tmpp = _tcschr (option, '.');
  if (tmpp) {
    section = option;
    option = tmpp + 1;
    *tmpp = '\0';
    if (_tcscmp (section, TARGET_NAME) == 0) {
	    /* We special case the various path options here.  */
      if (cfgfile_multipath (option, value, _T("rom_path"), &p->path_rom, p)
  		|| cfgfile_multipath (option, value, _T("floppy_path"), &p->path_floppy, p)
  		|| cfgfile_multipath (option, value, _T("cd_path"), &p->path_cd, p)
  		|| cfgfile_multipath (option, value, _T("hardfile_path"), &p->path_hardfile, p))
	    	return 1;
  	  return target_parse_option (p, option, value);
  	}
  	return 0;
  }

	for (i = 0; i < MAX_TOTAL_SCSI_DEVICES; i++) {
		TCHAR tmp[20];
		_stprintf (tmp, _T("cdimage%d"), i);
		if (!_tcsicmp (option, tmp)) {
			if (!_tcsicmp (value, _T("autodetect"))) {
				p->cdslots[i].type = SCSI_UNIT_DEFAULT;
				p->cdslots[i].inuse = true;
				p->cdslots[i].name[0] = 0;
			} else {
				p->cdslots[i].delayed = false;
				TCHAR *next = _tcsrchr (value, ',');
				int type = SCSI_UNIT_DEFAULT;
				int unitnum = 0;
				for (;;) {
					if (!next)
						break;
					*next++ = 0;
					TCHAR *next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					if (!_tcsicmp (next, _T("delay"))) {
						p->cdslots[i].delayed = true;
						next = next2;
						if (!next)
							break;
						next2 = _tcschr (next, ':');
						if (next2)
							*next2++ = 0;
					}
					type = match_string (cdmodes, next);
					if (type < 0)
						type = SCSI_UNIT_DEFAULT;
					else
						type--;
					next = next2;
					if (!next)
						break;
					next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					next = next2;
					if (!next)
						break;
					next2 = _tcschr (next, ':');
					if (next2)
						*next2++ = 0;
					cfgfile_intval (option, next, tmp, &unitnum, 1);
				}
				if (_tcslen (value) > 0) {
					_tcsncpy (p->cdslots[i].name, value, sizeof(p->cdslots[i].name) / sizeof (TCHAR));
				}
				p->cdslots[i].name[sizeof(p->cdslots[i].name) / sizeof(TCHAR) - 1] = 0;
				p->cdslots[i].inuse = true;
				p->cdslots[i].type = type;
				if (value[0] == 0 || !_tcsicmp(value, _T("empty")) || !_tcscmp(value, _T("."))) {
					value[0] = 0;
					p->cdslots[i].name[0] = 0;
					p->cdslots[i].inuse = false;
				}
			}
			// disable all following units
			i++;
			while (i < MAX_TOTAL_SCSI_DEVICES) {
				p->cdslots[i].type = SCSI_UNIT_DISABLED;
				i++;
			}
			return 1;
		}
	}

  if (cfgfile_intval (option, value, _T("sound_frequency"), &p->sound_freq, 1)
		|| cfgfile_intval (option, value, _T("sound_volume_paula"), &p->sound_volume_paula, 1)
		|| cfgfile_intval (option, value, _T("sound_volume_cd"), &p->sound_volume_cd, 1)
	  || cfgfile_intval (option, value, _T("sound_stereo_separation"), &p->sound_stereo_separation, 1)
	  || cfgfile_intval (option, value, _T("sound_stereo_mixing_delay"), &p->sound_mixed_stereo_delay, 1)

	  || cfgfile_intval (option, value, _T("gfx_framerate"), &p->gfx_framerate, 1)
		|| cfgfile_intval(option, value, _T("gfx_top_windowed"), &p->gfx_monitor.gfx_size.y, 1)
		|| cfgfile_intval(option, value, _T("gfx_left_windowed"), &p->gfx_monitor.gfx_size.x, 1)
		|| cfgfile_intval (option, value, _T("gfx_refreshrate"), &p->gfx_apmode[APMODE_NATIVE].gfx_refreshrate, 1)
		|| cfgfile_intval (option, value, _T("gfx_refreshrate_rtg"), &p->gfx_apmode[APMODE_RTG].gfx_refreshrate, 1)

		|| cfgfile_intval (option, value, _T("filesys_max_size"), &p->filesys_limit, 1)
		|| cfgfile_intval (option, value, _T("filesys_max_name_length"), &p->filesys_max_name, 1)

		|| cfgfile_intval (option, value, _T("floppy0sound"), &p->floppyslots[0].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy1sound"), &p->floppyslots[1].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy2sound"), &p->floppyslots[2].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy3sound"), &p->floppyslots[3].dfxclick, 1)
		|| cfgfile_intval (option, value, _T("floppy0soundvolume_disk"), &p->dfxclickvolume_disk[0], 1)
		|| cfgfile_intval (option, value, _T("floppy1soundvolume_disk"), &p->dfxclickvolume_disk[1], 1)
		|| cfgfile_intval (option, value, _T("floppy2soundvolume_disk"), &p->dfxclickvolume_disk[2], 1)
		|| cfgfile_intval (option, value, _T("floppy3soundvolume_disk"), &p->dfxclickvolume_disk[3], 1)
		|| cfgfile_intval (option, value, _T("floppy0soundvolume_empty"), &p->dfxclickvolume_empty[0], 1)
		|| cfgfile_intval (option, value, _T("floppy1soundvolume_empty"), &p->dfxclickvolume_empty[1], 1)
		|| cfgfile_intval (option, value, _T("floppy2soundvolume_empty"), &p->dfxclickvolume_empty[2], 1)
		|| cfgfile_intval (option, value, _T("floppy3soundvolume_empty"), &p->dfxclickvolume_empty[3], 1))
	  return 1;

	if (cfgfile_string (option, value, _T("config_info"), p->info, sizeof p->info / sizeof (TCHAR))
	  || cfgfile_string (option, value, _T("config_description"), p->description, sizeof p->description / sizeof (TCHAR)))
	  return 1;

	if (cfgfile_yesno (option, value, _T("floppy0wp"), &p->floppyslots[0].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy1wp"), &p->floppyslots[1].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy2wp"), &p->floppyslots[2].forcedwriteprotect)
		|| cfgfile_yesno (option, value, _T("floppy3wp"), &p->floppyslots[3].forcedwriteprotect)
		|| cfgfile_yesno(option, value, _T("warp"), &p->turbo_emulation)
    || cfgfile_yesno (option, value, _T("bsdsocket_emu"), &p->socket_emu))
	  return 1;

  if (cfgfile_strval (option, value, _T("sound_output"), &p->produce_sound, soundmode1, 1)
	  || cfgfile_strval (option, value, _T("sound_output"), &p->produce_sound, soundmode2, 0)
	  || cfgfile_strval (option, value, _T("sound_interpol"), &p->sound_interpol, interpolmode, 0)
	  || cfgfile_strval (option, value, _T("sound_filter"), &p->sound_filter, soundfiltermode1, 0)
	  || cfgfile_strval (option, value, _T("sound_filter_type"), &p->sound_filter_type, soundfiltermode2, 0)
	  || cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode1, 1)
	  || cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode2, 1)
	  || cfgfile_strboolval (option, value, _T("use_gui"), &p->start_gui, guimode3, 0)
	  || cfgfile_strval (option, value, _T("gfx_resolution"), &p->gfx_resolution, lorestype1, 0)
	  || cfgfile_strval (option, value, _T("gfx_lores"), &p->gfx_resolution, lorestype2, 0)
	  || cfgfile_strval (option, value, _T("absolute_mouse"), &p->input_tablet, abspointers, 0))
    return 1;
	
	if (cfgfile_intval (option, value, _T("floppy_volume"), &v, 1)) {
		for (int i = 0; i < 4; i++) {
			p->dfxclickvolume_disk[i] = v;
			p->dfxclickvolume_empty[i] = v;
		}
		return 1;
	}

	if (_tcscmp (option, _T("gfx_width_windowed")) == 0) {
		return 1;
	}
	if (_tcscmp (option, _T("gfx_height_windowed")) == 0) {
		return 1;
	}
	if (_tcscmp (option, _T("gfx_width_fullscreen")) == 0) {
		return 1;
	}
	if (_tcscmp (option, _T("gfx_height_fullscreen")) == 0) {
		return 1;
	}

	if (_tcscmp (option, _T("gfx_linemode")) == 0) {
		int v = 0;
		p->gfx_vresolution = VRES_DOUBLE;
		if (cfgfile_strval(option, value, _T("gfx_linemode"), &v, linemode, 0)) {
			p->gfx_vresolution = VRES_NONDOUBLE;
			if (v > 0) {
				p->gfx_vresolution = VRES_DOUBLE;
			}
		}
		return 1;
	}

  if(cfgfile_yesno (option, value, _T("show_leds"), &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_CHIPSET;
		else
			p->leds_on_screen &= ~STATUSLINE_CHIPSET;
		return 1;
  }
	if (cfgfile_yesno (option, value, _T("show_leds_rtg"), &vb)) {
		if (vb)
			p->leds_on_screen |= STATUSLINE_RTG;
		else
			p->leds_on_screen &= ~STATUSLINE_RTG;
		return 1;
	}
	if (_tcscmp (option, _T("show_leds_enabled")) == 0 || _tcscmp (option, _T("show_leds_enabled_rtg")) == 0) {
		TCHAR tmp[MAX_DPATH];
		int idx = _tcscmp (option, _T("show_leds_enabled")) == 0 ? 0 : 1;
		p->leds_on_screen_mask[idx] = 0;
		_tcscpy (tmp, value);
		_tcscat (tmp, _T(","));
		TCHAR *s = tmp;
		for (;;) {
			TCHAR *s2 = s;
			TCHAR *s3 = _tcschr (s, ':');
			s = _tcschr (s, ',');
			if (!s)
				break;
			if (s3 && s3 < s)
				s = s3;
			*s = 0;
			for (int i = 0; leds[i]; i++) {
				if (!_tcsicmp (s2, leds[i])) {
					p->leds_on_screen_mask[idx] |= 1 << i;
				}
			}
			s++;
		}
		return 1;
	}

  if (_tcscmp (option, _T("gfx_width")) == 0 || _tcscmp (option, _T("gfx_height")) == 0) {
	  cfgfile_intval (option, value, _T("gfx_width"), &p->gfx_monitor.gfx_size.width, 1);
	  cfgfile_intval (option, value, _T("gfx_height"), &p->gfx_monitor.gfx_size.height, 1);
	  return 1;
  }

	if (_tcscmp (option, _T("joyportfriendlyname0")) == 0 || _tcscmp (option, _T("joyportfriendlyname1")) == 0) {
		inputdevice_joyport_config_store(p, value, _tcscmp (option, _T("joyportfriendlyname0")) == 0 ? 0 : 1, -1, 2);
		return 1;
	}
	if (_tcscmp (option, _T("joyportfriendlyname2")) == 0 || _tcscmp (option, _T("joyportfriendlyname3")) == 0) {
		inputdevice_joyport_config_store(p, value, _tcscmp (option, _T("joyportfriendlyname2")) == 0 ? 2 : 3, -1, 2);
		return 1;
	}
	if (_tcscmp (option, _T("joyportname0")) == 0 || _tcscmp (option, _T("joyportname1")) == 0) {
		inputdevice_joyport_config_store(p, value, _tcscmp (option, _T("joyportname0")) == 0 ? 0 : 1, -1, 1);
		return 1;
	}
	if (_tcscmp (option, _T("joyportname2")) == 0 || _tcscmp (option, _T("joyportname3")) == 0) {
		inputdevice_joyport_config_store(p, value, _tcscmp (option, _T("joyportname2")) == 0 ? 2 : 3, -1, 1);
		return 1;
	}
	if (_tcscmp (option, _T("joyport0")) == 0 || _tcscmp (option, _T("joyport1")) == 0) {
    int port = _tcscmp (option, _T("joyport0")) == 0 ? 0 : 1;
		inputdevice_joyport_config_store(p, _T(""), port, -1, 1);
		inputdevice_joyport_config_store(p, _T(""), port, -1, 2);
		inputdevice_joyport_config_store(p, value, port, -1, 0);
		return 1;
	}
	if (_tcscmp (option, _T("joyport2")) == 0 || _tcscmp (option, _T("joyport3")) == 0) {
    int port = _tcscmp (option, _T("joyport2")) == 0 ? 2 : 3;
		inputdevice_joyport_config_store(p, _T(""), port, -1, 1);
		inputdevice_joyport_config_store(p, _T(""), port, -1, 2);
		inputdevice_joyport_config_store(p, value, port, -1, 0);
		return 1;
	}
	if (cfgfile_strval (option, value, _T("joyport0mode"), &p->jports[0].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport1mode"), &p->jports[1].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport2mode"), &p->jports[2].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport3mode"), &p->jports[3].mode, joyportmodes, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport0autofire"), &p->jports[0].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport1autofire"), &p->jports[1].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport2autofire"), &p->jports[2].autofire, joyaf, 0))
		return 1;
	if (cfgfile_strval (option, value, _T("joyport3autofire"), &p->jports[3].autofire, joyaf, 0))
		return 1;

	if (cfgfile_yesno (option, value, _T("joyport0keyboardoverride"), &vb)) {
		p->jports[0].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport1keyboardoverride"), &vb)) {
		p->jports[1].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport2keyboardoverride"), &vb)) {
		p->jports[2].nokeyboardoverride = !vb;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("joyport3keyboardoverride"), &vb)) {
		p->jports[3].nokeyboardoverride = !vb;
		return 1;
	}

  if (cfgfile_string (option, value, _T("statefile"), tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
	  _tcscpy (savestate_fname, tmpbuf);
	  if (zfile_exists (savestate_fname)) {
	    savestate_state = STATE_DORESTORE;
  	} else {
	    int ok = 0;
	    if (savestate_fname[0]) {
    		for (;;) {
  		    TCHAR *p;
  		    if (my_existsdir (savestate_fname)) {
		        ok = 1;
		        break;
  		    }
		      p = _tcsrchr (savestate_fname, '\\');
		      if (!p)
		        p = _tcsrchr (savestate_fname, '/');
  		    if (!p)
		        break;
  		    *p = 0;
    		}
	    }
	    if (!ok) {
    		savestate_fname[0] = 0;
			}
  	}
	  return 1;
  }

  if (cfgfile_strval (option, value, _T("sound_channels"), &p->sound_stereo, stereomode, 1)) {
	  if (p->sound_stereo == SND_NONE) { /* "mixed stereo" compatibility hack */
	    p->sound_stereo = SND_STEREO;
	    p->sound_mixed_stereo_delay = 5;
	    p->sound_stereo_separation = 7;
	  }
	  return 1;
  }

	if (_tcscmp (option, _T("kbd_lang")) == 0) {
		KbdLang l;
		if ((l = KBD_LANG_DE, _tcsicmp (value, _T("de")) == 0)
			|| (l = KBD_LANG_DK, _tcsicmp (value, _T("dk")) == 0)
			|| (l = KBD_LANG_SE, _tcsicmp (value, _T("se")) == 0)
			|| (l = KBD_LANG_US, _tcsicmp (value, _T("us")) == 0)
			|| (l = KBD_LANG_FR, _tcsicmp (value, _T("fr")) == 0)
			|| (l = KBD_LANG_IT, _tcsicmp (value, _T("it")) == 0)
			|| (l = KBD_LANG_ES, _tcsicmp (value, _T("es")) == 0))
			p->keyboard_lang = l;
		else
			cfgfile_warning(_T("Unknown keyboard language\n"));
		return 1;
	}

  if (cfgfile_string (option, value, _T("config_version"), tmpbuf, sizeof (tmpbuf) / sizeof (TCHAR))) {
  	TCHAR *tmpp2;
	  tmpp = _tcschr (value, '.');
	  if (tmpp) {
	    *tmpp++ = 0;
	    tmpp2 = tmpp;
	    p->config_version = _tstol (tmpbuf) << 16;
	    tmpp = _tcschr (tmpp, '.');
	    if (tmpp) {
	    	*tmpp++ = 0;
	    	p->config_version |= _tstol (tmpp2) << 8;
	    	p->config_version |= _tstol (tmpp);
	    }
	  }
	  return 1;
  }


  return 0;
}

static void decode_rom_ident (TCHAR *romfile, int maxlen, const TCHAR *ident, int romflags)
{
	const TCHAR *p;
	int ver, rev, subver, subrev, round, i;
	TCHAR model[64], *modelp;
	struct romlist **rl;
	TCHAR *romtxt;

	if (!ident[0])
		return;
	romtxt = xmalloc (TCHAR, 10000);
	romtxt[0] = 0;
	for (round = 0; round < 2; round++) {
		ver = rev = subver = subrev = -1;
		modelp = NULL;
		memset (model, 0, sizeof model);
		p = ident;
		while (*p) {
			TCHAR c = *p++;
			int *pp1 = NULL, *pp2 = NULL;
			if (_totupper (c) == 'V' && _istdigit (*p)) {
				pp1 = &ver;
				pp2 = &rev;
			} else if (_totupper (c) == 'R' && _istdigit (*p)) {
				pp1 = &subver;
				pp2 = &subrev;
			} else if (!_istdigit (c) && c != ' ') {
				_tcsncpy (model, p - 1, (sizeof model) / sizeof (TCHAR) - 1);
				p += _tcslen (model);
				modelp = model;
			}
			if (pp1) {
				*pp1 = _tstol (p);
				while (*p != 0 && *p != '.' && *p != ' ')
					p++;
				if (*p == '.') {
					p++;
					if (pp2)
						*pp2 = _tstol (p);
				}
			}
			if (*p == 0 || *p == ';') {
				rl = getromlistbyident (ver, rev, subver, subrev, modelp, romflags, round > 0);
				if (rl) {
					for (i = 0; rl[i]; i++) {
						if (rl[i]->path && !_tcscmp(rl[i]->path, romfile)) {
							xfree(rl);
							round = 0;
							goto end;
						}
					}
					if (!rl[i]) {
						for (i = 0; rl[i]; i++) {
							if (round) {
								TCHAR romname[MAX_DPATH];
								getromname(rl[i]->rd, romname);
								_tcscat (romtxt, romname);
								_tcscat (romtxt, _T("\n"));
							} else {
								_tcsncpy (romfile, rl[i]->path, maxlen);
								goto end;
							}
						}
					}
					xfree (rl);
				}
			}
		}
	}
end:
	if (round && romtxt[0]) {
		notify_user_parms (NUMSG_ROMNEED, romtxt, romtxt);
	}
	xfree (romtxt);
}

static struct uaedev_config_data *getuci(struct uae_prefs *p)
{
  if (p->mountitems < MOUNT_CONFIG_SIZE)
  	return &p->mountconfig[p->mountitems++];
  return NULL;
}

struct uaedev_config_data *add_filesys_config (struct uae_prefs *p, int index, struct uaedev_config_info *ci)
{
	struct uaedev_config_data *uci;
  int i;

	if (index < 0 && (ci->type == UAEDEV_DIR || ci->type == UAEDEV_HDF) && ci->devname && _tcslen (ci->devname) > 0) {
		for (i = 0; i < p->mountitems; i++) {
			if (p->mountconfig[i].ci.devname && !_tcscmp (p->mountconfig[i].ci.devname, ci->devname))
				return NULL;
		}
	}

  if (index < 0) {
		if (ci->controller_type != HD_CONTROLLER_TYPE_UAE) {
			int ctrl = ci->controller_type;
			int ctrlunit = ci->controller_type_unit;
			int cunit = ci->controller_unit;
			for (;;) {
				for (i = 0; i < p->mountitems; i++) {
					if (p->mountconfig[i].ci.controller_type == ctrl && p->mountconfig[i].ci.controller_type_unit == ctrlunit && p->mountconfig[i].ci.controller_unit == cunit) {
						cunit++;
						if (ctrl >= HD_CONTROLLER_TYPE_IDE_FIRST && ctrl <= HD_CONTROLLER_TYPE_IDE_LAST && cunit == 4)
							return NULL;
					}
				}
				if (i == p->mountitems) {
					ci->controller_unit = cunit;
					break;
				}
			}
		} else if (ci->controller_type == HD_CONTROLLER_TYPE_UAE) {
			int ctrl = ci->controller_type;
			int ctrlunit = ci->controller_type_unit;
			int cunit = ci->controller_unit;
			for (;;) {
				for (i = 0; i < p->mountitems; i++) {
					if (p->mountconfig[i].ci.controller_type == ctrl && p->mountconfig[i].ci.controller_type_unit == ctrlunit && p->mountconfig[i].ci.controller_unit == cunit) {
						cunit++;
						if (cunit >= MAX_FILESYSTEM_UNITS)
							return NULL;
					}
				}
				if (i == p->mountitems) {
					ci->controller_unit = cunit;
					break;
				}
			}
		}
	  uci = getuci(p);
	  uci->configoffset = -1;
  } else {
  	uci = &p->mountconfig[index];
  }
  if (!uci)
		return NULL;

	memcpy (&uci->ci, ci, sizeof (struct uaedev_config_info));
	validatedevicename (uci->ci.devname, NULL);
	validatevolumename (uci->ci.volname, NULL);
	if (!uci->ci.devname[0]) {
  	  TCHAR base[32];
	  TCHAR base2[32];
	  int num = 0;
		if (uci->ci.rootdir[0] == 0 && ci->type == UAEDEV_DIR)
		  _tcscpy (base, _T("RDH"));
	  else
		  _tcscpy (base, _T("DH"));
	  _tcscpy (base2, base);
	  for (i = 0; i < p->mountitems; i++) {
		  _stprintf (base2, _T("%s%d"), base, num);
			if (!_tcsicmp(base2, p->mountconfig[i].ci.devname)) {
		    num++;
		    i = -1;
		    continue;
      }
	  }
		_tcscpy (uci->ci.devname, base2);
		validatedevicename (uci->ci.devname, NULL);
  }
	if (ci->type == UAEDEV_DIR) {
		TCHAR *s = filesys_createvolname (uci->ci.volname, uci->ci.rootdir, NULL, _T("Harddrive"));
		_tcscpy (uci->ci.volname, s);
    xfree (s);
	}
  return uci;
}

static void parse_addmem (struct uae_prefs *p, TCHAR *buf, int num)
{
	int size = 0, addr = 0;

	if (!getintval2 (&buf, &addr, ',', false))
		return;
	if (!getintval2 (&buf, &size, 0, true))
		return;
	if (addr & 0xffff)
		return;
	if ((size & 0xffff) || (size & 0xffff0000) == 0)
		return;
	p->custom_memory_addrs[num] = addr;
	p->custom_memory_sizes[num] = size;
}

static void get_filesys_controller (const TCHAR *hdc, int *type, int *typenum, int *num)
{
	int hdcv = HD_CONTROLLER_TYPE_UAE;
	int hdunit = 0;
	int idx = 0;
	if(_tcslen (hdc) >= 4 && !_tcsncmp (hdc, _T("ide"), 3)) {
		hdcv = HD_CONTROLLER_TYPE_IDE_AUTO;
		hdunit = hdc[3] - '0';
		if (hdunit < 0 || hdunit >= 6)
			hdunit = 0;
	}
	if (hdcv == HD_CONTROLLER_TYPE_UAE) {
		hdunit = _tstol(hdc + 3);
		if (hdunit >= MAX_FILESYSTEM_UNITS)
			hdunit = 0;
	} else if (hdcv > HD_CONTROLLER_TYPE_UAE) {
		TCHAR control[MAX_DPATH];
		bool found = false;
		_tcscpy(control, hdc);
		TCHAR *extend = (TCHAR*)_tcschr(control, ',');
		if (extend)
			extend[0] = 0;
		const TCHAR *ext = _tcsrchr(control, '_');
		if (ext) {
			ext++;
			int len = _tcslen(ext);
			if (len > 2 && ext[len - 2] == '-' && ext[len - 1] >= '2' && ext[len - 1] <= '9') {
				idx = ext[len - 1] - '1';
				len -= 2;
			}
			for (int i = 0; hdcontrollers[i].label; i++) {
				const TCHAR *ext2 = _tcsrchr(hdcontrollers[i].label, '_');
				if (ext2) {
					ext2++;
					if (_tcslen(ext2) == len && !_tcsnicmp(ext, ext2, len) && hdc[0] == hdcontrollers[i].label[0]) {
						if (hdcontrollers[i].romtype) {
							for (int j = 0; expansionroms[j].name; j++) {
								if ((expansionroms[j].romtype & ROMTYPE_MASK) == hdcontrollers[i].romtype) {
									if (hdcv == HD_CONTROLLER_TYPE_IDE_AUTO) {
									  hdcv = j + HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST;
									}
									break;
								}
							}
						}
						if (hdcv == HD_CONTROLLER_TYPE_IDE_AUTO) {
						  hdcv = i;
						}
						found = true;
						break;
					}
				}
			}
			if (!found) {
				for (int i = 0; expansionroms[i].name; i++) {
					const struct expansionromtype *ert = &expansionroms[i];
					if (_tcslen(ert->name) == len && !_tcsnicmp(ext, ert->name, len)) {
						if (hdcv == HD_CONTROLLER_TYPE_IDE_AUTO) {
							hdcv = HD_CONTROLLER_TYPE_IDE_EXPANSION_FIRST + i;
						}
						break;
					}
				}

			}
		}
	}
	if (idx >= MAX_DUPLICATE_EXPANSION_BOARDS)
		idx = MAX_DUPLICATE_EXPANSION_BOARDS - 1;
	if (hdunit < 0)
		hdunit = 0;
	*type = hdcv;
	*typenum = idx;
	*num = hdunit;
}

static int cfgfile_parse_partial_newfilesys (struct uae_prefs *p, int nr, int type, const TCHAR *value, int unit, bool uaehfentry)
{
	TCHAR *tmpp;
	TCHAR *name = NULL, *path = NULL;

	// read only harddrive name
	if (!uaehfentry)
		return 0;
	if (type != 1)
		return 0;
	tmpp = getnextentry (&value, ',');
	if (!tmpp)
		return 0;
	xfree (tmpp);
	tmpp = getnextentry (&value, ':');
	if (!tmpp)
		return 0;
	xfree (tmpp);
	name = getnextentry (&value, ':');
	if (name && _tcslen (name) > 0) {
		path = getnextentry (&value, ',');
		if (path && _tcslen (path) > 0) {
			for (int i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
				struct uaedev_config_info *uci = &p->mountconfig[i].ci;
				if (_tcsicmp (uci->rootdir, name) == 0) {
					_tcscat (uci->rootdir, _T(":"));
					_tcscat (uci->rootdir, path);
				}
			}
		}
	}
	xfree (path);
	xfree (name);
	return 1;
}

static int cfgfile_parse_newfilesys (struct uae_prefs *p, int nr, int type, TCHAR *value, int unit, bool uaehfentry)
{
	struct uaedev_config_info uci;
	TCHAR *tmpp = _tcschr (value, ','), *tmpp2;
  TCHAR *str = NULL;
	TCHAR devname[MAX_DPATH], volname[MAX_DPATH];

	devname[0] = volname[0] = 0;
	uci_set_defaults (&uci);

  config_newfilesystem = 1;
  if (tmpp == 0)
    goto invalid_fs;

  *tmpp++ = '\0';
  if (_tcsicmp (value, _T("ro")) == 0)
		uci.readonly = true;
  else if (_tcsicmp (value, _T("rw")) == 0)
		uci.readonly = false;
	else
    goto invalid_fs;

  value = tmpp;
	if (type == 0) {
		uci.type = UAEDEV_DIR;
    tmpp = _tcschr (value, ':');
    if (tmpp == 0)
  		goto empty_fs;
    *tmpp++ = 0;
		_tcscpy (devname, value);
		tmpp2 = tmpp;
    tmpp = _tcschr (tmpp, ':');
    if (tmpp == 0)
  		goto empty_fs;
    *tmpp++ = 0;
		_tcscpy (volname, tmpp2);
		tmpp2 = tmpp;
		// quoted special case
		if (tmpp2[0] == '\"') {
			const TCHAR *end;
			TCHAR *n = cfgfile_unescape (tmpp2, &end, 0, false);
			if (!n)
				goto invalid_fs;
			_tcscpy (uci.rootdir, n);
			xfree(n);
			tmpp = (TCHAR*)end;
			*tmpp++ = 0;
		} else {
      tmpp = _tcschr (tmpp, ',');
      if (tmpp == 0)
    		goto empty_fs;
      *tmpp++ = 0;
		  _tcscpy (uci.rootdir, tmpp2);
		}
		_tcscpy (uci.volname, volname);
		_tcscpy (uci.devname, devname);
		if (! getintval (&tmpp, &uci.bootpri, 0))
  		goto empty_fs;
	} else if (type == 1 || ((type == 2 || type == 3) && uaehfentry)) {
    tmpp = _tcschr (value, ':');
    if (tmpp == 0)
	    goto invalid_fs;
    *tmpp++ = '\0';
		_tcscpy (devname, value);
		tmpp2 = tmpp;
		// quoted special case
		if (tmpp2[0] == '\"') {
			const TCHAR *end;
			TCHAR *n = cfgfile_unescape (tmpp2, &end, 0, false);
			if (!n)
				goto invalid_fs;
			_tcscpy (uci.rootdir, n);
			xfree(n);
			tmpp = (TCHAR*)end;
			*tmpp++ = 0;
		} else {
      tmpp = _tcschr (tmpp, ',');
      if (tmpp == 0)
	      goto invalid_fs;
      *tmpp++ = 0;
		  _tcscpy (uci.rootdir, tmpp2);
		}
		_tcscpy (uci.devname, devname);
		if (! getintval (&tmpp, &uci.sectors, ',')
			|| ! getintval (&tmpp, &uci.surfaces, ',')
			|| ! getintval (&tmpp, &uci.reserved, ',')
			|| ! getintval (&tmpp, &uci.blocksize, ','))
	    goto invalid_fs;
		if (getintval2 (&tmpp, &uci.bootpri, ',', false)) {
			tmpp2 = tmpp;
			// quoted special case
			if (tmpp2[0] == '\"') {
				const TCHAR *end;
				TCHAR *n = cfgfile_unescape (tmpp2, &end, 0, false);
				if (!n)
					goto invalid_fs;
				xfree(n);
				tmpp = (TCHAR*)end;
				*tmpp++ = 0;
			} else {
	      tmpp = _tcschr (tmpp, ',');
				if (tmpp == 0)
					goto empty_fs;
        *tmpp++ = 0;
			}
		  get_filesys_controller (tmpp, &uci.controller_type, &uci.controller_type_unit, &uci.controller_unit);
			tmpp2 = _tcschr (tmpp, ',');
			if (tmpp2) {
				tmpp2++;
				if (getintval2 (&tmpp2, &uci.highcyl, ',', false)) {
					uci.pheads = uci.psecs = uci.pcyls = 0;
				}
			}
		  uci.unit_feature_level = 1;

		  TCHAR *pflags;
		  if ((pflags = cfgfile_option_get(tmpp2, _T("flags")))) {
			  getintval(&pflags, &uci.unit_special_flags, 0);
				xfree(pflags);
		  }

			if (cfgfile_option_find(tmpp2, _T("lock")))
				uci.lock = true;

			if (cfgfile_option_find(tmpp2, _T("SCSI2")))
				uci.unit_feature_level = HD_LEVEL_SCSI_2;
			else if (cfgfile_option_find(tmpp2, _T("SCSI1")))
				uci.unit_feature_level = HD_LEVEL_SCSI_1;
			else if (cfgfile_option_find(tmpp2, _T("ATA2+S")))
				uci.unit_feature_level = HD_LEVEL_ATA_2S;
			else if (cfgfile_option_find(tmpp2, _T("ATA2+")))
				uci.unit_feature_level = HD_LEVEL_ATA_2;
			else if (cfgfile_option_find(tmpp2, _T("ATA1")))
				uci.unit_feature_level = HD_LEVEL_ATA_1;
    }
		uci.type = UAEDEV_HDF;
	} else {
		goto invalid_fs;
	}
empty_fs:
	if (uci.rootdir[0]) {
		if (_tcslen (uci.rootdir) > 3 && uci.rootdir[0] == 'H' && uci.rootdir[1] == 'D' && uci.rootdir[2] == '_') {
			memmove (uci.rootdir, uci.rootdir + 2, (_tcslen (uci.rootdir + 2) + 1) * sizeof (TCHAR));
			uci.rootdir[0] = ':';
		}
	}
#ifdef FILESYS
	add_filesys_config (p, nr, &uci);
#endif
  xfree (str);
  return 1;

invalid_fs:
	cfgfile_warning(_T("Invalid filesystem/hardfile/cd specification.\n"));
	return 1;
}

static int cfgfile_parse_filesys (struct uae_prefs *p, const TCHAR *option, TCHAR *value)
{
	int i;

  for (i = 0; i < MAX_FILESYSTEM_UNITS; i++) {
	  TCHAR tmp[100];
	  _stprintf (tmp, _T("uaehf%d"), i);
		if (!_tcscmp (option, tmp)) {
			for (;;) {
				int  type = -1;
				int unit = -1;
				TCHAR *tmpp = _tcschr (value, ',');
				if (tmpp == NULL)
	        return 1;
				*tmpp++ = 0;
				if (_tcsicmp (value, _T("hdf")) == 0) {
					type = 1;
					cfgfile_parse_partial_newfilesys (p, -1, type, tmpp, unit, true);
					return 1;
				} else if (_tcsnicmp (value, _T("cd"), 2) == 0 && (value[2] == 0 || value[3] == 0)) {
					unit = 0;
					if (value[2] > 0)
						unit = value[2] - '0';
					if (unit >= 0 && unit <= MAX_TOTAL_SCSI_DEVICES) {
						type = 2;
					}
				} else if (_tcsicmp (value, _T("dir")) != 0) {
					type = 0;
					return 1;  /* ignore for now */
				}
				if (type >= 0)
					cfgfile_parse_newfilesys (p, -1, type, tmpp, unit, true);
				return 1;
			}
			return 1;
		} else if (!_tcsncmp (option, tmp, _tcslen (tmp)) && option[_tcslen (tmp)] == '_') {
			struct uaedev_config_info *uci = &currprefs.mountconfig[i].ci;
			if (uci->devname) {
				const TCHAR *s = &option[_tcslen (tmp) + 1];
				if (!_tcscmp (s, _T("bootpri"))) {
					getintval (&value, &uci->bootpri, 0);
				} else if (!_tcscmp (s, _T("read-only"))) {
					cfgfile_yesno (NULL, value, NULL, &uci->readonly);
				} else if (!_tcscmp (s, _T("volumename"))) {
					_tcscpy (uci->volname, value);
				} else if (!_tcscmp (s, _T("devicename"))) {
					_tcscpy (uci->devname, value);
				} else if (!_tcscmp (s, _T("root"))) {
					_tcscpy (uci->rootdir, value);
				} else if (!_tcscmp (s, _T("controller"))) {
					get_filesys_controller (value, &uci->controller_type, &uci->controller_type_unit, &uci->controller_unit);
				}
			}
		}
  }

  if (_tcscmp (option, _T("filesystem")) == 0
	|| _tcscmp (option, _T("hardfile")) == 0)
  {
		struct uaedev_config_info uci;
	  TCHAR *tmpp = _tcschr (value, ',');
	  TCHAR *str;
		bool hdf;

		uci_set_defaults (&uci);

	  if (config_newfilesystem)
	    return 1;

	  if (tmpp == 0)
	    goto invalid_fs;

	  *tmpp++ = '\0';
	  if (_tcscmp (value, _T("1")) == 0 || _tcsicmp (value, _T("ro")) == 0
    || _tcsicmp (value, _T("readonly")) == 0
    || _tcsicmp (value, _T("read-only")) == 0)
			uci.readonly = true;
  	else if (_tcscmp (value, _T("0")) == 0 || _tcsicmp (value, _T("rw")) == 0
		|| _tcsicmp (value, _T("readwrite")) == 0
		|| _tcsicmp (value, _T("read-write")) == 0)
			uci.readonly = false;
	  else
	    goto invalid_fs;

  	value = tmpp;
	  if (_tcscmp (option, _T("filesystem")) == 0) {
			hdf = false;
	    tmpp = _tcschr (value, ':');
      if (tmpp == 0)
    		goto invalid_fs;
      *tmpp++ = '\0';
			_tcscpy (uci.volname, value);
			_tcscpy (uci.rootdir, tmpp);
	  } else {
			hdf = true;
			if (! getintval (&value, &uci.sectors, ',')
				|| ! getintval (&value, &uci.surfaces, ',')
				|| ! getintval (&value, &uci.reserved, ',')
				|| ! getintval (&value, &uci.blocksize, ','))
    		goto invalid_fs;
			_tcscpy (uci.rootdir, value);
  	}
		str = cfgfile_subst_path_load (UNEXPANDED, &p->path_hardfile, uci.rootdir, true);
#ifdef FILESYS
		uci.type = hdf ? UAEDEV_HDF : UAEDEV_DIR;
		add_filesys_config (p, -1, &uci);
#endif
		xfree (str);
	  return 1;
invalid_fs:
		cfgfile_warning(_T("Invalid filesystem/hardfile specification.\n"));
  	return 1;
	}

	if (_tcscmp (option, _T("filesystem2")) == 0)
		return cfgfile_parse_newfilesys (p, -1, 0, value, -1, false);
	if (_tcscmp (option, _T("hardfile2")) == 0)
		return cfgfile_parse_newfilesys (p, -1, 1, value, -1, false);

	return 0;
}

static bool cfgfile_read_board_rom(struct uae_prefs *p, const TCHAR *option, const TCHAR *value, struct multipath *mp)
{
	TCHAR buf[256], buf2[MAX_DPATH], buf3[MAX_DPATH];
	bool dummy;
	int val;
	const struct expansionromtype *ert;

	for (int i = 0; expansionroms[i].name; i++) {
		struct boardromconfig *brc; 
		int idx;
		ert = &expansionroms[i];

		for (int j = 0; j < MAX_DUPLICATE_EXPANSION_BOARDS; j++) {
			TCHAR name[256];

			if (j == 0)
				_tcscpy(name, ert->name);
			else
				_stprintf(name, _T("%s-%d"), ert->name, j + 1);

			_stprintf(buf, _T("scsi_%s"), name);
			if (cfgfile_yesno(option, value, buf, &dummy)) {
				return true;
			}

			_stprintf(buf, _T("%s_rom_file"), name);
			if (cfgfile_path(option, value, buf, buf2, MAX_DPATH / sizeof (TCHAR), mp)) {
				if (buf2[0]) {
					brc = get_device_rom_new(p, ert->romtype, j, &idx);
					_tcscpy(brc->roms[idx].romfile, buf2);
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom_file_id"), name);
			buf2[0] = 0;
			if (cfgfile_rom (option, value, buf, buf2, MAX_DPATH / sizeof (TCHAR))) {
				if (buf2[0]) {
					brc = get_device_rom_new(p, ert->romtype, j, &idx);
					_tcscpy(brc->roms[idx].romfile, buf2);
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom"), name);
			if (cfgfile_string (option, value, buf, buf2, sizeof buf2 / sizeof (TCHAR))) {
				if (buf2[0]) {
					decode_rom_ident (buf3, sizeof(buf3) / sizeof (TCHAR), buf2, ert->romtype);
					if (buf3[0]) {
						brc = get_device_rom_new(p, ert->romtype, j, &idx);
						_tcscpy(brc->roms[idx].romident, buf3);
					}
				}
				return true;
			}

			_stprintf(buf, _T("%s_rom_options"), name);
			if (cfgfile_string (option, value, buf, buf2, sizeof buf2 / sizeof (TCHAR))) {
				brc = get_device_rom(p, ert->romtype, j, &idx);
				if (brc) {
					TCHAR *p;
					if (cfgfile_option_bool(buf2, _T("inserted")) == 1) {
						brc->roms[idx].inserted= true;
					}
					p = cfgfile_option_get(buf2, _T("order"));
					if (p) {
						brc->device_order = _tstol(p);
					}
					xfree(p);
				}
				return true;
			}
		}

		_stprintf(buf, _T("%s_mem_size"), ert->name);
		if (cfgfile_intval (option, value, buf, &val, 0x40000)) {
			if (val) {
				brc = get_device_rom_new(p, ert->romtype, 0, &idx);
				brc->roms[idx].board_ram_size = val;
			}
			return true;
		}
	}
	return false;
}

static void addbcromtype(struct uae_prefs *p, int romtype, bool add, const TCHAR *romfile, int devnum)
{
	if (!add) {
		clear_device_rom(p, romtype, devnum, true);
	} else {
		struct boardromconfig *brc = get_device_rom_new(p, romtype, devnum, NULL);
		if (brc && !brc->roms[0].romfile[0]) {
			_tcscpy(brc->roms[0].romfile, romfile ? romfile : _T(":ENABLED"));
		}
	}
}

static int cfgfile_parse_hardware (struct uae_prefs *p, const TCHAR *option, TCHAR *value)
{
  int tmpval, dummyint, i;
  TCHAR tmpbuf[CONFIG_BLEN];

	if (cfgfile_yesno(option, value, _T("cpu_compatible"), &p->cpu_compatible)) {
		return 1;
	}

	if (cfgfile_yesno (option, value, _T("cpu_cycle_exact"), &p->cpu_cycle_exact)) {
		/* we don't want cycle-exact in 68020/40+JIT modes */
		if (p->cpu_model >= 68020 && p->cachesize > 0)
			p->cpu_cycle_exact = p->cpu_memory_cycle_exact = p->blitter_cycle_exact = 0;
		p->cpu_memory_cycle_exact = p->cpu_cycle_exact;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("blitter_cycle_exact"), &p->blitter_cycle_exact)) {
		if (p->cpu_model >= 68020 && p->cachesize > 0)
			p->cpu_cycle_exact = p->cpu_memory_cycle_exact = p->blitter_cycle_exact = 0;
		return 1;
	}
	if (cfgfile_yesno (option, value, _T("cpu_memory_cycle_exact"), &p->cpu_memory_cycle_exact)) {
		if (!p->cpu_memory_cycle_exact)
			p->blitter_cycle_exact = p->cpu_cycle_exact = false;
		return 1;
	}
	if (cfgfile_strval (option, value, _T("cycle_exact"), &tmpval, cycleexact, 0)) {
		if (tmpval > 0) {
			p->blitter_cycle_exact = true;
			p->cpu_cycle_exact = tmpval > 1;
			p->cpu_memory_cycle_exact = true;
		} else {
			p->blitter_cycle_exact = false;
			p->cpu_cycle_exact = false;
			p->cpu_memory_cycle_exact = false;
		}
		if (p->cpu_model >= 68020 && p->cachesize > 0)
			p->cpu_cycle_exact = p->cpu_memory_cycle_exact = p->blitter_cycle_exact = false;
		return 1;
	}

  if (cfgfile_yesno (option, value, _T("immediate_blits"), &p->immediate_blits)
	  || cfgfile_yesno (option, value, _T("fast_copper"), &p->fast_copper)
		|| cfgfile_yesno(option, value, _T("fpu_no_unimplemented"), &p->fpu_no_unimplemented)
		|| cfgfile_yesno (option, value, _T("cd32cd"), &p->cs_cd32cd)
		|| cfgfile_yesno (option, value, _T("cd32c2p"), &p->cs_cd32c2p)
		|| cfgfile_yesno (option, value, _T("cd32nvram"), &p->cs_cd32nvram)
		|| cfgfile_yesno(option, value, _T("cia_overlay"), &p->cs_ciaoverlay)
		|| cfgfile_yesno(option, value, _T("ksmirror_e0"), &p->cs_ksmirror_e0)
		|| cfgfile_yesno(option, value, _T("ksmirror_a8"), &p->cs_ksmirror_a8)
		|| cfgfile_yesno(option, value, _T("cia_todbug"), &p->cs_ciatodbug)
		|| cfgfile_yesno(option, value, _T("z3_autoconfig"), &p->cs_z3autoconfig)

	  || cfgfile_yesno (option, value, _T("ntsc"), &p->ntscmode)
	  || cfgfile_yesno (option, value, _T("cpu_24bit_addressing"), &p->address_space_24)
		|| cfgfile_yesno (option, value, _T("fpu_strict"), &p->fpu_strict)
#ifdef USE_JIT_FPU
		|| cfgfile_yesno (option, value, _T("compfpu"), &p->compfpu)
#endif
		|| cfgfile_yesno (option, value, _T("floppy_write_protect"), &p->floppy_read_only)
		|| cfgfile_yesno(option, value, _T("harddrive_write_protect"), &p->harddrive_read_only))
	  return 1;

  if (cfgfile_intval (option, value, _T("cachesize"), &p->cachesize, 1)
		|| cfgfile_intval (option, value, _T("cd32nvram_size"), &p->cs_cd32nvram_size, 1024)
		|| cfgfile_intval (option, value, _T("fatgary"), &p->cs_fatgaryrev, 1)
		|| cfgfile_intval (option, value, _T("ramsey"), &p->cs_ramseyrev, 1)
	  || cfgfile_doubleval (option, value, _T("chipset_refreshrate"), &p->chipset_refreshrate)
		|| cfgfile_intval (option, value, _T("a3000mem_size"), &p->mbresmem_low.size, 0x100000)
		|| cfgfile_intval (option, value, _T("mbresmem_size"), &p->mbresmem_high.size, 0x100000)
	  || cfgfile_intval (option, value, _T("z3mem_start"), &p->z3autoconfig_start, 1)
	  || cfgfile_intval (option, value, _T("bogomem_size"), &p->bogomem.size, 0x40000)
	  || cfgfile_intval (option, value, _T("rtg_modes"), &p->picasso96_modeflags, 1)
	  || cfgfile_intval (option, value, _T("floppy_speed"), &p->floppy_speed, 1)
		|| cfgfile_intval (option, value, _T("cd_speed"), &p->cd_speed, 1)
	  || cfgfile_intval (option, value, _T("floppy_write_length"), &p->floppy_write_length, 1)
	  || cfgfile_intval (option, value, _T("nr_floppies"), &p->nr_floppies, 1)
	  || cfgfile_intval (option, value, _T("floppy0type"), &p->floppyslots[0].dfxtype, 1)
	  || cfgfile_intval (option, value, _T("floppy1type"), &p->floppyslots[1].dfxtype, 1)
	  || cfgfile_intval (option, value, _T("floppy2type"), &p->floppyslots[2].dfxtype, 1)
	  || cfgfile_intval (option, value, _T("floppy3type"), &p->floppyslots[3].dfxtype, 1))
	  return 1;

  if (cfgfile_strval (option, value, _T("rtc"), &p->cs_rtc, rtctype, 0)
		|| cfgfile_strval (option, value, _T("ciaatod"), &p->cs_ciaatod, ciaatodmode, 0)
		|| cfgfile_strval (option, value, _T("scsi"), &p->scsi, scsimode, 0)
    || cfgfile_strval (option, value, _T("collision_level"), &p->collision_level, collmode, 0)
		|| cfgfile_strval (option, value, _T("waiting_blits"), &p->waiting_blits, waitblits, 0)
		|| cfgfile_strval (option, value, _T("floppy_auto_extended_adf"), &p->floppy_auto_ext2, autoext2, 0)
		|| cfgfile_strval (option, value,  _T("z3mapping"), &p->z3_mapping_mode, z3mapping, 0)
		|| cfgfile_strval(option, value, _T("boot_rom_uae"), &p->boot_rom, uaebootrom, 0)
		|| cfgfile_strval(option, value, _T("unmapped_address_space"), &p->cs_unmapped_space, unmapped, 0)
		|| cfgfile_strval(option, value, _T("ciaa_type"), &p->cs_ciatype[0], ciatype, 0)
		|| cfgfile_strval(option, value, _T("ciab_type"), &p->cs_ciatype[1], ciatype, 0))
  	return 1;

	if (cfgfile_strval(option, value, _T("uaeboard"), &p->uaeboard, uaeboard, 0)) {
		return 1;
	}

  if (cfgfile_path (option, value, _T("kickstart_rom_file"), p->romfile, sizeof p->romfile / sizeof (TCHAR), &p->path_rom)
	  || cfgfile_path (option, value, _T("kickstart_ext_rom_file"), p->romextfile, sizeof p->romextfile / sizeof (TCHAR), &p->path_rom)
		|| cfgfile_rom(option, value, _T("kickstart_rom_file_id"), p->romfile, sizeof p->romfile / sizeof(TCHAR))
		|| cfgfile_rom (option, value, _T("kickstart_ext_rom_file_id"), p->romextfile, sizeof p->romextfile / sizeof (TCHAR))
    || cfgfile_string (option, value, _T("flash_file"), p->flashfile, sizeof p->flashfile / sizeof (TCHAR))
    || cfgfile_path (option, value, _T("cart_file"), p->cartfile, sizeof p->cartfile / sizeof (TCHAR), &p->path_rom))
	  return 1;

	if (cfgfile_string(option, value, _T("uaeboard_options"), tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		TCHAR *s = cfgfile_option_get(value, _T("order"));
		if (s)
			p->uaeboard_order = _tstol(s);
		xfree(s);
		return 1;
	}

  if (cfgfile_readramboard(option, value, _T("chipmem"), &p->chipmem)) {
		return 1;
	}
	if (cfgfile_readramboard(option, value, _T("bogomem"), &p->bogomem)) {
		return 1;
	}
	if (cfgfile_readramboard(option, value, _T("fastmem"), &p->fastmem[0])) {
		return 1;
	}
  if (cfgfile_readramboard(option, value, _T("a3000mem"), &p->mbresmem_low)) {
		return 1;
	}
	if (cfgfile_readramboard(option, value, _T("mbresmem"), &p->mbresmem_high)) {
		return 1;
	}
	if (cfgfile_readramboard(option, value, _T("z3mem"), &p->z3fastmem[0])) {
		return 1;
	}

	if (cfgfile_yesno(option, value, _T("pcmcia"), &p->cs_pcmcia)) {
		if (p->cs_pcmcia)
			addbcromtype(p, ROMTYPE_MB_PCMCIA, true, NULL, 0);
		return 1;
	}
	if (cfgfile_strval(option, value, _T("ide"), &p->cs_ide, idemode, 0)) {
		if (p->cs_ide)
			addbcromtype(p, ROMTYPE_MB_IDE, true, NULL, 0);
		return 1;
	}
	if (cfgfile_yesno(option, value, _T("cd32fmv"), &p->cs_cd32fmv)) {
		if (p->cs_cd32fmv) {
			addbcromtype(p, ROMTYPE_CD32CART, true, p->cartfile, 0);
		}
		return 1;
	}

	struct rtgboardconfig *rbc = &p->rtgboards[0];
	TCHAR tmp[100];
	_tcscpy(tmp, _T("gfxcard_size"));
	if (cfgfile_intval(option, value, tmp, &rbc->rtgmem_size, 0x100000))
		return 1;
	_tcscpy(tmp, _T("gfxcard_options"));
	if (!_tcsicmp(option, tmp)) {
		TCHAR *s = cfgfile_option_get(value, _T("order"));
		if (s) {
			rbc->device_order = _tstol(s);
			xfree(s);
		}
		return 1;
	}
	_tcscpy(tmp, _T("gfxcard_type"));
	if (cfgfile_string(option, value, tmp, tmpbuf, sizeof tmpbuf / sizeof(TCHAR))) {
		rbc->rtgmem_type = 0;
		int j = 0;
		for (;;) {
			const TCHAR *t = gfxboard_get_configname(j);
			if (!t) {
				break;
			}
			if (!_tcsicmp(t, tmpbuf)) {
				rbc->rtgmem_type = j;
				break;
			}
			j++;
		}
		return 1;
	}

	if (cfgfile_strval (option, value, _T("chipset_compatible"), &p->cs_compatible, cscompa, 0)) {
		built_in_chipset_prefs (p);
		return 1;
	}

	if (cfgfile_strval (option, value, _T("cart_internal"), &p->cart_internal, cartsmode, 0)) {
		if (p->cart_internal) {
			struct romdata *rd = getromdatabyid (63);
			if (rd)
				_stprintf (p->cartfile, _T(":%s"), rd->configname);
		}
		return 1;
	}
	if (cfgfile_string (option, value, _T("kickstart_rom"), p->romident, sizeof p->romident / sizeof (TCHAR))) {
		decode_rom_ident (p->romfile, sizeof p->romfile / sizeof (TCHAR), p->romident, ROMTYPE_ALL_KICK);
		return 1;
	}
	if (cfgfile_string (option, value, _T("kickstart_ext_rom"), p->romextident, sizeof p->romextident / sizeof (TCHAR))) {
		decode_rom_ident (p->romextfile, sizeof p->romextfile / sizeof (TCHAR), p->romextident, ROMTYPE_ALL_EXT);
		return 1;
	}

	if (cfgfile_string (option, value, _T("cart"), p->cartident, sizeof p->cartident / sizeof (TCHAR))) {
		decode_rom_ident (p->cartfile, sizeof p->cartfile / sizeof (TCHAR), p->cartident, ROMTYPE_ALL_CART);
		return 1;
	}

	if (cfgfile_read_board_rom(p, option, value, &p->path_rom))
		return 1;

  for (i = 0; i < 4; i++) {
	  _stprintf (tmpbuf, _T("floppy%d"), i);
		if (cfgfile_string(option, value, tmpbuf, p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof(TCHAR))) {
			if (!_tcscmp(p->floppyslots[i].df, _T(".")))
				p->floppyslots[i].df[0] = 0;
	    return 1;
		}
  }

  if (cfgfile_intval (option, value, _T("chipmem_size"), &dummyint, 1)) {
  	if (dummyint < 0)
	    p->chipmem.size = 0x20000; /* 128k, prototype support */
  	else if (dummyint == 0)
	    p->chipmem.size = 0x40000; /* 256k */
  	else
	    p->chipmem.size = dummyint * 0x80000;
  	return 1;
  }

	if (cfgfile_string (option, value, _T("addmem1"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 0);
		return 1;
	}
	if (cfgfile_string (option, value, _T("addmem2"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		parse_addmem (p, tmpbuf, 1);
		return 1;
	}

  if (cfgfile_strval (option, value, _T("chipset"), &tmpval, csmode, 0)) {
    set_chipset_mask (p, tmpval);
    return 1;
  }

  if (cfgfile_string (option, value, _T("fpu_model"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	  p->fpu_model = _tstol(tmpbuf);
	  return 1;
  }

	if (cfgfile_string (option, value, _T("cpu_model"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
	  p->cpu_model = _tstol(tmpbuf);
	  p->fpu_model = 0;
	  return 1;
  }

    /* old-style CPU configuration */
	if (cfgfile_string (option, value, _T("cpu_type"), tmpbuf, sizeof tmpbuf / sizeof (TCHAR))) {
		// 68000/010 32-bit addressing was not available until 2.8.2
		bool force24bit = p->config_version <= ((2 << 16) | (8 << 8) | (1 << 0));
	  p->fpu_model = 0;
	  p->address_space_24 = 0;
	  p->cpu_model = 680000;
		if (!_tcscmp (tmpbuf, _T("68000"))) {
	    p->cpu_model = 68000;
			if (force24bit)
				p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, _T("68010"))) {
	    p->cpu_model = 68010;
			if (force24bit)
				p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, _T("68ec020"))) {
	    p->cpu_model = 68020;
		} else if (!_tcscmp (tmpbuf, _T("68020"))) {
	    p->cpu_model = 68020;
		} else if (!_tcscmp (tmpbuf, _T("68ec020/68881"))) {
	    p->cpu_model = 68020;
	    p->fpu_model = 68881;
	    p->address_space_24 = 1;
		} else if (!_tcscmp (tmpbuf, _T("68020/68881"))) {
	    p->cpu_model = 68020;
	    p->fpu_model = 68881;
		} else if (!_tcscmp (tmpbuf, _T("68040"))) {
	    p->cpu_model = 68040;
	    p->fpu_model = 68040;
  	}
	  return 1;
  }
    
	    /* Broken earlier versions used to write this out as a string.  */
	if (cfgfile_strval (option, value, _T("finegraincpu_speed"), &p->m68k_speed, speedmode, 1)) {
    p->m68k_speed--;
    return 1;
  }

	if (cfgfile_strval (option, value, _T("cpu_speed"), &p->m68k_speed, speedmode, 1)) {
		p->m68k_speed--;
		return 1;
	}
  if (cfgfile_intval (option, value, _T("cpu_speed"), &p->m68k_speed, 1)) {
    p->m68k_speed *= CYCLE_UNIT;
  	return 1;
  }
  if (cfgfile_intval (option, value, _T("finegrain_cpu_speed"), &p->m68k_speed, 1)) {
  	if (OFFICIAL_CYCLE_UNIT > CYCLE_UNIT) {
	    int factor = OFFICIAL_CYCLE_UNIT / CYCLE_UNIT;
	    p->m68k_speed = (p->m68k_speed + factor - 1) / factor;
  	}
    if (_tcsicmp (value, _T("max")) == 0)
      p->m68k_speed = -1;
  	return 1;
  }

	if (_tcsicmp (option, _T("quickstart")) == 0) {
		int model = 0;
		TCHAR *tmpp = _tcschr (value, ',');
		if (tmpp) {
			*tmpp++ = 0;
			TCHAR *tmpp2 = _tcschr (value, ',');
			if (tmpp2)
				*tmpp2 = 0;
			cfgfile_strval (option, value, option, &model, qsmodes,  0);
			if (model >= 0) {
				int config = _tstol (tmpp);
				built_in_prefs (p, model, config, 0, 0);
			}
		}
  	return 1;
  }

	if (cfgfile_parse_filesys (p, option, value))
		return 1;

  return 0;
}

void cfgfile_compatibility_romtype(struct uae_prefs *p)
{
	addbcromtype(p, ROMTYPE_MB_PCMCIA, p->cs_pcmcia, NULL, 0);	

	addbcromtype(p, ROMTYPE_MB_IDE, p->cs_ide != 0, NULL, 0);

	addbcromtype(p, ROMTYPE_CD32CART, p->cs_cd32fmv, p->cartfile,0);
}

static int getconfigstoreline (const TCHAR *option, TCHAR *value);

static void calcformula (struct uae_prefs *prefs, TCHAR *in)
{
	TCHAR out[MAX_DPATH], configvalue[CONFIG_BLEN];
	TCHAR *p = out;
	double val;
	int cnt1, cnt2;
	static bool updatestore;

	if (_tcslen (in) < 2 || in[0] != '[' || in[_tcslen (in) - 1] != ']')
		return;
	if (!configstore || updatestore)
		cfgfile_createconfigstore (prefs);
	updatestore = false;
	if (!configstore)
		return;
	cnt1 = cnt2 = 0;
	for (int i = 1; i < _tcslen (in) - 1; i++) {
		TCHAR c = _totupper (in[i]);
		if (c >= 'A' && c <='Z') {
			TCHAR *start = &in[i];
			while (_istalnum (c) || c == '_' || c == '.') {
				i++;
				c = in[i];
			}
			TCHAR store = in[i];
			in[i] = 0;
			if (!getconfigstoreline (start, configvalue))
				return;
			_tcscpy (p, configvalue);
			p += _tcslen (p);
			in[i] = store;
			i--;
			cnt1++;
		} else {
			cnt2++;
			*p ++= c;
		}
	}
	*p = 0;
	if (cnt1 == 0 && cnt2 == 0)
		return;
	/* single config entry only? */
	if (cnt1 == 1 && cnt2 == 0) {
		_tcscpy (in, out);
		updatestore = true;
		return;
	}
	if (calc (out, &val)) {
		if (val - (int)val != 0.0f)
			_stprintf (in, _T("%f"), val);
		else
		  _stprintf (in, _T("%d"), (int)val);
		updatestore = true;
		return;
	}
}

int cfgfile_parse_option (struct uae_prefs *p, const TCHAR *option, TCHAR *value, int type)
{
	calcformula (p, value);

  if (!_tcscmp (option, _T("config_hardware")))
  	return 1;
  if (!_tcscmp (option, _T("config_host")))
  	return 1;
  if (type == 0 || (type & CONFIG_TYPE_HARDWARE)) {
  	if (cfgfile_parse_hardware (p, option, value))
	    return 1;
  }
  if (type == 0 || (type & CONFIG_TYPE_HOST)) {
		// cfgfile_parse_host may modify the option (convert to lowercase).
		TCHAR* writable_option = my_strdup(option);
		if (cfgfile_parse_host (p, writable_option, value)) {
			free(writable_option);
	    return 1;
		}
		free(writable_option);
  }
	if (type > 0 && (type & (CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST)) != (CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST))
  	return 1;
  return 0;
}

static int isutf8ext (TCHAR *s)
{
	if (_tcslen (s) > _tcslen (UTF8NAME) && !_tcscmp (s + _tcslen (s) - _tcslen (UTF8NAME), UTF8NAME)) {
		s[_tcslen (s) - _tcslen (UTF8NAME)] = 0;
		return 1;
	}
	return 0;
}

int cfgfile_separate_linea (const TCHAR *filename, char *line, TCHAR *line1b, TCHAR *line2b)
{
  char *line1, *line2;
  int i;

  line1 = line;
  line1 += strspn (line1, "\t \r\n");
  if (*line1 == ';')
	  return 0;
  line2 = strchr (line, '=');
  if (! line2) {
		TCHAR *s = au (line1);
		cfgfile_warning(_T("CFGFILE: '%s', linea was incomplete with only %s\n"), filename, s);
		xfree (s);
  	return 0;
  }
  *line2++ = '\0';

  /* Get rid of whitespace.  */
  i = strlen (line2);
  while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
    || line2[i - 1] == '\r' || line2[i - 1] == '\n'))
	  line2[--i] = '\0';
  line2 += strspn (line2, "\t \r\n");

  i = strlen (line);
  while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
    || line[i - 1] == '\r' || line[i - 1] == '\n'))
  	line[--i] = '\0';
  line += strspn (line, "\t \r\n");
	au_copy (line1b, MAX_DPATH, line);
	if (isutf8ext (line1b)) {
		if (line2[0]) {
			TCHAR *s = utf8u (line2);
			_tcscpy (line2b, s);
			xfree (s);
		}
	} else {
		au_copy (line2b, MAX_DPATH, line2);
	}
  return 1;
}

static int cfgfile_separate_line (TCHAR *line, TCHAR *line1b, TCHAR *line2b)
{
	TCHAR *line1, *line2;
	int i;

	line1 = line;
	line1 += _tcsspn (line1, _T("\t \r\n"));
	if (*line1 == ';')
		return 0;
	line2 = _tcschr (line, '=');
	if (! line2) {
		cfgfile_warning(_T("CFGFILE: line was incomplete with only %s\n"), line1);
		return 0;
	}
	*line2++ = '\0';

	/* Get rid of whitespace.  */
	i = _tcslen (line2);
	while (i > 0 && (line2[i - 1] == '\t' || line2[i - 1] == ' '
		|| line2[i - 1] == '\r' || line2[i - 1] == '\n'))
		line2[--i] = '\0';
	line2 += _tcsspn (line2, _T("\t \r\n"));
	_tcscpy (line2b, line2);
	i = _tcslen (line);
	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ' '
		|| line[i - 1] == '\r' || line[i - 1] == '\n'))
		line[--i] = '\0';
	line += _tcsspn (line, _T("\t \r\n"));
	_tcscpy (line1b, line);

	if (line2b[0] == '"' || line2b[0] == '\"') {
		TCHAR c = line2b[0];
		int i = 0;
		memmove (line2b, line2b + 1, (_tcslen (line2b) + 1) * sizeof (TCHAR));
		while (line2b[i] != 0 && line2b[i] != c)
			i++;
		line2b[i] = 0;
	}

	if (isutf8ext (line1b))
		return 0;
	return 1;
}

static int isobsolete (TCHAR *s)
{
  int i = 0;
  while (obsolete[i]) {
  	if (!_tcsicmp (s, obsolete[i])) {
			cfgfile_warning_obsolete(_T("obsolete config entry '%s'\n"), s);
	    return 1;
  	}
  	i++;
  }
  if (_tcslen (s) > 2 && !_tcsncmp (s, _T("w."), 2))
  	return 1;
  if (_tcslen (s) >= 10 && !_tcsncmp (s, _T("gfx_opengl"), 10)) {
		cfgfile_warning_obsolete(_T("obsolete config entry '%s\n"), s);
  	return 1;
  }
  if (_tcslen (s) >= 6 && !_tcsncmp (s, _T("gfx_3d"), 6)) {
		cfgfile_warning_obsolete(_T("obsolete config entry '%s\n"), s);
    return 1;
  }
  return 0;
}

static void cfgfile_parse_separated_line (struct uae_prefs *p, TCHAR *line1b, TCHAR *line2b, int type)
{
  TCHAR line3b[CONFIG_BLEN], line4b[CONFIG_BLEN];
  struct strlist *sl;
  int ret;

  _tcscpy (line3b, line1b);
  _tcscpy (line4b, line2b);
  ret = cfgfile_parse_option (p, line1b, line2b, type);
  if (!isobsolete (line3b)) {
  	for (sl = p->all_lines; sl; sl = sl->next) {
	    if (sl->option && !_tcsicmp (line1b, sl->option)) break;
  	}
  	if (!sl) {
	    struct strlist *u = xcalloc (struct strlist, 1);
	    u->option = my_strdup(line3b);
	    u->value = my_strdup(line4b);
	    u->next = p->all_lines;
	    p->all_lines = u;
	    if (!ret) {
		    u->unknown = 1;
				cfgfile_warning(_T("unknown config entry: '%s=%s'\n"), u->option, u->value);
	    }
  	}
  }
}

void cfgfile_parse_line (struct uae_prefs *p, TCHAR *line, int type)
{
  TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

  if (!cfgfile_separate_line (line, line1b, line2b))
  	return;
  cfgfile_parse_separated_line (p, line1b, line2b, type);
}

static void subst (TCHAR *p, TCHAR *f, int n)
{
	if (_tcslen(p) == 0 || _tcslen(f) == 0)
		return;
  TCHAR *str = cfgfile_subst_path (UNEXPANDED, p, f);
  _tcsncpy (f, str, n - 1);
  f[n - 1] = '\0';
  free (str);
}

static int getconfigstoreline (const TCHAR *option, TCHAR *value)
{
	TCHAR tmp[CONFIG_BLEN * 2], tmp2[CONFIG_BLEN * 2];

	if (!configstore)
		return 0;
	zfile_fseek (configstore, 0, SEEK_SET);
	for (;;) {
		if (!zfile_fgets (tmp, sizeof tmp / sizeof (TCHAR), configstore))
			return 0;
		if (!cfgfile_separate_line (tmp, tmp2, value))
			continue;
		if (!_tcsicmp (option, tmp2))
			return 1;
	}
}

bool cfgfile_createconfigstore (struct uae_prefs *p)
{
	uae_u8 zeros[4] = { 0 };
	zfile_fclose (configstore);
	configstore = zfile_fopen_empty (NULL, _T("configstore"), 50000);
	if (!configstore)
		return false;
	zfile_fseek (configstore, 0, SEEK_SET);
	uaeconfig++;
	cfgfile_save_options (configstore, p, 0);
	uaeconfig--;
	zfile_fwrite (zeros, 1, sizeof zeros, configstore);
	zfile_truncate(configstore, zfile_ftell(configstore));
	zfile_fseek (configstore, 0, SEEK_SET);
	return true;
}

static char *cfg_fgets (char *line, int max, struct zfile *fh)
{
  if (fh)
  	return zfile_fgetsa (line, max, fh);
  return 0;
}

static int cfgfile_load_2 (struct uae_prefs *p, const TCHAR *filename, bool real, int *type)
{
  int i;
  struct zfile *fh;
  char linea[CONFIG_BLEN];
  TCHAR line[CONFIG_BLEN], line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];
  struct strlist *sl;
  bool type1 = false, type2 = false;
  int askedtype = 0;

  if (type) {
	  askedtype = *type;
	  *type = 0;
  }
  if (real) {
	  p->config_version = 0;
	  config_newfilesystem = 0;
	  //reset_inputdevice_config (p);
  }

  fh = zfile_fopen (filename, _T("r"), ZFD_NORMAL);
  if (! fh)
  	return 0;

  while (cfg_fgets (linea, sizeof (linea), fh) != 0) {
  	trimwsa (linea);
  	if (strlen (linea) > 0) {
			if (linea[0] == '#' || linea[0] == ';') {
				struct strlist *u = xcalloc (struct strlist, 1);
				u->option = NULL;
				TCHAR *com = au (linea);
				u->value = my_strdup (com);
				xfree (com);
				u->unknown = 1;
				u->next = p->all_lines;
				p->all_lines = u;
		    continue;
			}
	    if (!cfgfile_separate_linea (filename, linea, line1b, line2b))
    		continue;
	    type1 = type2 = 0;
	    if (cfgfile_yesno (line1b, line2b, _T("config_hardware"), &type1) ||
        cfgfile_yesno (line1b, line2b, _T("config_host"), &type2)) {
  	    	if (type1 && type)
	    	    *type |= CONFIG_TYPE_HARDWARE;
  	    	if (type2 && type)
    		    *type |= CONFIG_TYPE_HOST;
      		continue;
	    }
	    if (real) {
    		cfgfile_parse_separated_line (p, line1b, line2b, askedtype);
	    } else {
				// metadata
    		cfgfile_string (line1b, line2b, _T("config_description"), p->description, sizeof p->description / sizeof (TCHAR));
	    }
	  }
  }

  if (type && *type == 0)
  	*type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
  zfile_fclose (fh);

  if (!real)
  	return 1;

  for (sl = temp_lines; sl; sl = sl->next) {
  	_stprintf (line, _T("%s=%s"), sl->option, sl->value);
  	cfgfile_parse_line (p, line, 0);
  }

  for (i = 0; i < 4; i++) {
  	subst (p->path_floppy.path[0], p->floppyslots[i].df, sizeof p->floppyslots[i].df / sizeof (TCHAR));
  	if(i >= p->nr_floppies)
  	  p->floppyslots[i].dfxtype = DRV_NONE;
  }
  subst (p->path_rom.path[0], p->romfile, sizeof p->romfile / sizeof (TCHAR));
  subst (p->path_rom.path[0], p->romextfile, sizeof p->romextfile / sizeof (TCHAR));

	for (i = 0; i < MAX_EXPANSION_BOARDS; i++) {
		for (int j = 0; j < MAX_BOARD_ROMS; j++) {
			subst(p->path_rom.path[0], p->expansionboard[i].roms[j].romfile, MAX_DPATH / sizeof(TCHAR));
		}
	}

  return 1;
}

int cfgfile_load (struct uae_prefs *p, const TCHAR *filename, int *type, int userconfig)
{
  int v;
  static int recursive;

  if (recursive > 1)
  	return 0;
  recursive++;
	write_log (_T("load config '%s':%d\n"), filename, type ? *type : -1);
  v = cfgfile_load_2 (p, filename, 1, type);
  if (!v) {
		cfgfile_warning(_T("cfgfile_load_2 failed\n"));
	  goto end;
  }
	if (userconfig)
		target_addtorecent (filename, 0);
end:
  recursive--;
	fixup_prefs (p, userconfig != 0);
  return v;
}

int cfgfile_save (struct uae_prefs *p, const TCHAR *filename, int type)
{
  struct zfile *fh;

  fh = zfile_fopen (filename, _T("w"), ZFD_NORMAL);
  if (! fh)
  	return 0;

  if (!type)
  	type = CONFIG_TYPE_HARDWARE | CONFIG_TYPE_HOST;
  cfgfile_save_options (fh, p, type);
  zfile_fclose (fh);
  return 1;
}

struct uae_prefs *cfgfile_open(const TCHAR *filename, int *type)
{
	struct uae_prefs *p = xcalloc(struct uae_prefs, 1);
	if (cfgfile_load_2(p, filename, false, type))
		return p;
	xfree(p);
	return NULL;
}

void cfgfile_close(struct uae_prefs *p)
{
	xfree(p);
}

int cfgfile_get_description (struct uae_prefs *p, const TCHAR *filename, TCHAR *description, int *type)
{
	bool alloc = false;

	if (!p) {
		p = xmalloc(struct uae_prefs, 1);
		alloc = true;
	}
	if (!p) {
		alloc = true;
		p = cfgfile_open(filename, type);
	}
	if (!p)
		return 0;
	if (description)
		_tcscpy (description, p->description);
	if (alloc) {
		cfgfile_close(p);
	}
	return 1;
}

static void parse_sound_spec (struct uae_prefs *p, const TCHAR *spec)
{
  TCHAR *x0 = my_strdup (spec);
	TCHAR *x1, *x2 = NULL, *x3 = NULL;

  x1 = _tcschr (x0, ':');
  if (x1 != NULL) {
	  *x1++ = '\0';
	  x2 = _tcschr (x1 + 1, ':');
	  if (x2 != NULL) {
	    *x2++ = '\0';
	    x3 = _tcschr (x2 + 1, ':');
	    if (x3 != NULL) {
		    *x3++ = '\0';
	    }
  	}
  }
  p->produce_sound = _tstoi (x0);
  if (x1) {
  	p->sound_stereo_separation = 0;
  	if (*x1 == 'S') {
	    p->sound_stereo = SND_STEREO;
	    p->sound_stereo_separation = 7;
  	} else if (*x1 == 's')
	    p->sound_stereo = SND_STEREO;
  	else
	    p->sound_stereo = SND_MONO;
  }
  if (x3)
  	p->sound_freq = _tstoi (x3);
  free (x0);
}

static void parse_joy_spec (struct uae_prefs *p, const TCHAR *spec)
{
	int v0 = 2, v1 = 0;
	if (_tcslen(spec) != 2)
		goto bad;

	switch (spec[0]) {
	case '0': v0 = JSEM_JOYS; break;
	case '1': v0 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v0 = JSEM_MICE; break;
	case 'A': case 'a': v0 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v0 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v0 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}

	switch (spec[1]) {
	case '0': v1 = JSEM_JOYS; break;
	case '1': v1 = JSEM_JOYS + 1; break;
	case 'M': case 'm': v1 = JSEM_MICE; break;
	case 'A': case 'a': v1 = JSEM_KBDLAYOUT; break;
	case 'B': case 'b': v1 = JSEM_KBDLAYOUT + 1; break;
	case 'C': case 'c': v1 = JSEM_KBDLAYOUT + 2; break;
	default: goto bad;
	}
	if (v0 == v1)
		goto bad;
	/* Let's scare Pascal programmers */
	if (0)
bad:
	write_log (_T("Bad joystick mode specification. Use -J xy, where x and y\n")
		_T("can be 0 for joystick 0, 1 for joystick 1, M for mouse, and\n")
		_T("a, b or c for different keyboard settings.\n"));

	p->jports[0].id = v0;
	p->jports[1].id = v1;
}

static void parse_filesys_spec (struct uae_prefs *p, bool readonly, const TCHAR *spec)
{
	struct uaedev_config_info uci;
  TCHAR buf[256];
  TCHAR *s2;

	uci_set_defaults (&uci);
  _tcsncpy (buf, spec, 255); buf[255] = 0;
  s2 = _tcschr (buf, ':');
  if (s2) {
  	*s2++ = '\0';
#ifdef __DOS__
	  {
      TCHAR *tmp;

      while ((tmp = _tcschr (s2, '\\')))
    		*tmp = '/';
	  }
#endif
#ifdef FILESYS
		_tcscpy (uci.volname, buf);
		_tcscpy (uci.rootdir, s2);
		uci.readonly = readonly;
		uci.type = UAEDEV_DIR;
		add_filesys_config (p, -1, &uci);
#endif
  } else {
		write_log (_T("Usage: [-m | -M] VOLNAME:mount_point\n"));
  }
}

static void parse_hardfile_spec (struct uae_prefs *p, const TCHAR *spec)
{
	struct uaedev_config_info uci;
  TCHAR *x0 = my_strdup (spec);
  TCHAR *x1, *x2, *x3, *x4;

	uci_set_defaults (&uci);
  x1 = _tcschr (x0, ':');
  if (x1 == NULL)
  	goto argh;
  *x1++ = '\0';
  x2 = _tcschr (x1 + 1, ':');
  if (x2 == NULL)
  	goto argh;
  *x2++ = '\0';
  x3 = _tcschr (x2 + 1, ':');
  if (x3 == NULL)
  	goto argh;
  *x3++ = '\0';
  x4 = _tcschr (x3 + 1, ':');
  if (x4 == NULL)
  	goto argh;
  *x4++ = '\0';
#ifdef FILESYS
	_tcscpy (uci.rootdir, x4);
#endif
  free (x0);
  return;

 argh:
  free (x0);
	cfgfile_warning(_T("Bad hardfile parameter specified\n"));
  return;
}

static void parse_cpu_specs (struct uae_prefs *p, const TCHAR *spec)
{
  if (*spec < '0' || *spec > '4') {
		cfgfile_warning(_T("CPU parameter string must begin with '0', '1', '2', '3' or '4'.\n"));
	  return;
  }

  p->cpu_model = (*spec++) * 10 + 68000;
  p->address_space_24 = p->cpu_model < 68020;
  p->cpu_compatible = 0;
  while (*spec != '\0') {
	  switch (*spec) {
	  case 'a':
	    if (p->cpu_model < 68020)
				cfgfile_warning(_T("In 68000/68010 emulation, the address space is always 24 bit.\n"));
	    else if (p->cpu_model >= 68040)
				cfgfile_warning(_T("In 68040/060 emulation, the address space is always 32 bit.\n"));
	    else
		    p->address_space_24 = 1;
	    break;
	  case 'c':
	    if (p->cpu_model != 68000)
				cfgfile_warning(_T("The more compatible CPU emulation is only available for 68000\n")
				_T("emulation, not for 68010 upwards.\n"));
	    else
		    p->cpu_compatible = 1;
	    break;
	  default:
			cfgfile_warning(_T("Bad CPU parameter specified.\n"));
	    break;
	  }
	  spec++;
  }
}

static void cmdpath (TCHAR *dst, const TCHAR *src, int maxsz)
{
	TCHAR *s = target_expand_environment (src, NULL, 0);
	_tcsncpy (dst, s, maxsz);
	dst[maxsz] = 0;
	xfree (s);
}

/* Returns the number of args used up (0 or 1).  */
int parse_cmdline_option (struct uae_prefs *p, TCHAR c, const TCHAR *arg)
{
  struct strlist *u = xcalloc (struct strlist, 1);
	const TCHAR arg_required[] = _T("0123rKmMWSRJwnvCZUFbcl");

  if (_tcschr (arg_required, c) && ! arg) {
		write_log (_T("Missing argument for option `-%c'!\n"), c);
	  return 0;
  }

  u->option = xmalloc (TCHAR, 2);
  u->option[0] = c;
  u->option[1] = 0;
	if (arg)
    u->value = my_strdup(arg);
  u->next = p->all_lines;
  p->all_lines = u;

  switch (c) {
    case '0': cmdpath (p->floppyslots[0].df, arg, 255); break;
    case '1': cmdpath (p->floppyslots[1].df, arg, 255); break;
    case '2': cmdpath (p->floppyslots[2].df, arg, 255); break;
    case '3': cmdpath (p->floppyslots[3].df, arg, 255); break;
    case 'r': cmdpath (p->romfile, arg, 255); break;
    case 'K': cmdpath (p->romextfile, arg, 255); break;
    case 'm': case 'M': parse_filesys_spec (p, c == 'M', arg); break;
    case 'W': parse_hardfile_spec (p, arg); break;
    case 'S': parse_sound_spec (p, arg); break;
    case 'R': p->gfx_framerate = _tstoi (arg); break;
	  case 'J': parse_joy_spec (p, arg); break;

    case 'w': p->m68k_speed = _tstoi (arg); break;

    case 'G': p->start_gui = 0; break;

    case 'n':
	    if (_tcschr (arg, 'i') != 0)
	      p->immediate_blits = 1;
	    break;

    case 'v':
	    set_chipset_mask (p, _tstoi (arg));
	    break;

    case 'C':
	    parse_cpu_specs (p, arg);
	    break;

    case 'Z':
	    p->z3fastmem[0].size = _tstoi (arg) * 0x100000;
	    break;

    case 'U':
		  p->rtgboards[0].rtgmem_size = _tstoi (arg) * 0x100000;
	    break;

    case 'F':
	    p->fastmem[0].size = _tstoi (arg) * 0x100000;
	    break;

    case 'b':
	    p->bogomem.size = _tstoi (arg) * 0x40000;
	    break;

    case 'c':
	    p->chipmem.size = _tstoi (arg) * 0x80000;
	    break;

	  case 'l':
		  if (0 == _tcsicmp(arg, _T("de")))
			  p->keyboard_lang = KBD_LANG_DE;
		  else if (0 == _tcsicmp(arg, _T("dk")))
			  p->keyboard_lang = KBD_LANG_DK;
		  else if (0 == _tcsicmp(arg, _T("us")))
			  p->keyboard_lang = KBD_LANG_US;
		  else if (0 == _tcsicmp(arg, _T("se")))
			  p->keyboard_lang = KBD_LANG_SE;
		  else if (0 == _tcsicmp(arg, _T("fr")))
			  p->keyboard_lang = KBD_LANG_FR;
		  else if (0 == _tcsicmp(arg, _T("it")))
			  p->keyboard_lang = KBD_LANG_IT;
		  else if (0 == _tcsicmp(arg, _T("es")))
			  p->keyboard_lang = KBD_LANG_ES;
		  break;

    default:
		  write_log (_T("Unknown option `-%c'!\n"), c);
	    break;
  }
  return !! _tcschr (arg_required, c);
}

void cfgfile_addcfgparam (TCHAR *line)
{
  struct strlist *u;
  TCHAR line1b[CONFIG_BLEN], line2b[CONFIG_BLEN];

  if (!line) {
	  struct strlist **ps = &temp_lines;
	  while (*ps) {
	    struct strlist *s = *ps;
	    *ps = s->next;
	    xfree (s->value);
	    xfree (s->option);
	    xfree (s);
  	}
  	temp_lines = 0;
  	return;
  }
  if (!cfgfile_separate_line (line, line1b, line2b))
  	return;
	if (!_tcsnicmp(line1b, _T("input."), 6)) {
		line1b[5] = '_';
	}
  u = xcalloc (struct strlist, 1);
	if (u) {
    u->option = my_strdup(line1b);
    u->value = my_strdup(line2b);
    u->next = temp_lines;
    temp_lines = u;
	}
}

static int cmdlineparser (const TCHAR *s, TCHAR *outp[], int max)
{
	int j, cnt = 0;
	int slash = 0;
	int quote = 0;
	TCHAR tmp1[MAX_DPATH];
	const TCHAR *prev;
	int doout;

	doout = 0;
	prev = s;
	j = 0;
	outp[0] = 0;
	tmp1[0] = 0;
	while (cnt < max) {
		TCHAR c = *s++;
		if (!c)
			break;
		if (c < 32)
			continue;
		if (c == '\\')
			slash = 1;
		if (!slash && c == '"') {
			if (quote) {
				quote = 0;
				doout = 1;
			} else {
				quote = 1;
				j = -1;
			}
		}
		if (!quote && c == ' ')
			doout = 1;
		if (!doout) {
			if (j >= 0) {
				tmp1[j] = c;
				tmp1[j + 1] = 0;
			}
			j++;
		}
		if (doout) {
			if (_tcslen (tmp1) > 0) {
				outp[cnt++] = my_strdup (tmp1);
				outp[cnt] = 0;
			}
			tmp1[0] = 0;
			doout = 0;
			j = 0;
		}
		slash = 0;
	}
	if (j > 0 && cnt < max) {
		outp[cnt++] = my_strdup (tmp1);
		outp[cnt] = 0;
	}
	return cnt;
}

#define UAELIB_MAX_PARSE 100

static int cfgfile_searchconfig(const TCHAR *in, int index, TCHAR *out, int outsize)
{
	TCHAR tmp[CONFIG_BLEN];
	int j = 0;
	int inlen = _tcslen (in);
	int joker = 0;
	uae_u32 err = 0;
	bool configsearchfound = false;
	int index2 = index;

	if (in[inlen - 1] == '*') {
		joker = 1;
		inlen--;
	}
	*out = 0;

	if (!configstore)
		cfgfile_createconfigstore(&currprefs);
	if (!configstore)
		return 20;

	if (index < 0) {
		index = 0;
		zfile_fseek(configstore, 0, SEEK_SET);
	} else {
		// if seek position==0: configstore was reset, always start from the beginning.
		if (zfile_ftell(configstore) > 0)
			index = 0;
	}

	tmp[0] = 0;
	for (;;) {
		uae_u8 b = 0;

		if (zfile_fread (&b, 1, 1, configstore) != 1) {
			err = 10;
			if (configsearchfound || index2 > 0)
				err = 0;
			goto end;
		}
		if (j >= sizeof (tmp) / sizeof (TCHAR) - 1)
			j = sizeof (tmp) / sizeof (TCHAR) - 1;
		if (b == 0) {
			err = 10;
			if (configsearchfound || index2 > 0)
				err = 0;
			goto end;
		}
		if (b == '\n') {
			if (inlen >= 0 && !_tcsncmp (tmp, in, inlen) && ((inlen > 0 && _tcslen (tmp) > inlen && tmp[inlen] == '=') || (joker))) {
				if (index <= 0) {
				  TCHAR *p;
				  if (joker)
					  p = tmp - 1;
				  else
					  p = _tcschr (tmp, '=');
				  if (p) {
					  for (int i = 0; out && i < outsize - 1; i++) {
						  TCHAR b = *++p;
						  out[i] = b;
						  out[i + 1] = 0;
						  if (!b)
							  break;
					  }
				  }
				  err = 0xffffffff;
				  configsearchfound = true;
				  goto end;
				} else {
					index--;
				}
			}
			j = 0;
		} else {
			tmp[j++] = b;
			tmp[j] = 0;
		}
	}
end:
	return err;
}

static int execcmdline(struct uae_prefs *prefs, int argv, TCHAR **argc, TCHAR *out, int outsize, bool confonly)
{
	int ret = 0;
	bool changed = false;
	for (int i = 0; i < argv; i++) {
		if (i + 2 <= argv) {
			if (!confonly) {
			  if (!inputdevice_uaelib (argc[i], argc[i + 1])) {
				  if (!cfgfile_parse_option (prefs, argc[i], argc[i + 1], 0)) {
						ret = 5;
					  break;
				  }
					changed = true;
			  }
			} else {
				if (!cfgfile_parse_option(prefs, argc[i], argc[i + 1], 0)) {
					ret = 5;
					break;
				}
				changed = true;
			}
			i++;
		}
	}
	if (changed) {
		inputdevice_fix_prefs(prefs, false);
		set_config_changed ();
		set_special (SPCFLAG_MODE_CHANGE);
	}
	return ret;
}

static uae_u32 cfgfile_modify (uae_u32 index, const TCHAR *parms, uae_u32 size, TCHAR *out, uae_u32 outsize)
{
	TCHAR *p;
	TCHAR *argc[UAELIB_MAX_PARSE];
	int argv, i;
	uae_u32 err;
	static TCHAR *configsearch;

	if (out)
	  *out = 0;
	err = 0;
	argv = 0;
	p = 0;
	if (index != 0xffffffff) {
		if (!configstore) {
			err = 20;
			goto end;
		}
		if (configsearch) {
			err = cfgfile_searchconfig(configsearch, index,  out, outsize);
			goto end;
		}
		err = 0xffffffff;
		for (i = 0; out && i < outsize - 1; i++) {
			uae_u8 b = 0;
			if (zfile_fread (&b, 1, 1, configstore) != 1)
				err = 0;
			if (b == 0)
				err = 0;
			if (b == '\n')
				b = 0;
			out[i] = b;
			out[i + 1] = 0;
			if (!b)
				break;
		}
		goto end;
	}

	if (size > 10000)
		return 10;

	argv = cmdlineparser (parms, argc, UAELIB_MAX_PARSE);

	if (argv <= 1 && index == 0xffffffff) {
		cfgfile_createconfigstore (&currprefs);
		xfree (configsearch);
		configsearch = NULL;
		if (!configstore) {
			err = 20;
			goto end;
		}
		if (argv > 0 && _tcslen (argc[0]) > 0)
			configsearch = my_strdup (argc[0]);
		err = 0xffffffff;
		goto end;
	}

	err = execcmdline(&changed_prefs, argv, argc, out, outsize, false);
end:
	for (i = 0; i < argv; i++)
		xfree (argc[i]);
	xfree (p);
	return err;
}

uae_u32 cfgfile_uaelib_modify(TrapContext *ctx, uae_u32 index, uae_u32 parms, uae_u32 size, uae_u32 out, uae_u32 outsize)
{
	uae_char *p, *parms_p = NULL, *parms_out = NULL;
	int i, ret;
	TCHAR *out_p = NULL, *parms_in = NULL;

	if (outsize >= 32768)
		return 0;
	if (out && outsize > 0) {
		if (!trap_valid_address(ctx, out, 1))
			return 0;
		trap_put_byte(ctx, out, 0);
	}
	if (size == 0) {
		for (;;) {
			if (!trap_valid_address(ctx, parms + size, 1))
				return 0;
			if (trap_get_byte(ctx, parms + size) == 0)
				break;
			size++;
			if (size >= 32768)
				return 0;
		}
	}
	parms_p = xmalloc (uae_char, size + 1);
	if (!parms_p) {
		ret = 10;
		goto end;
	}
	if (out) {
		out_p = xmalloc (TCHAR, outsize + 1);
		if (!out_p) {
			ret = 10;
			goto end;
		}
		out_p[0] = 0;
	}
	p = parms_p;
	for (i = 0; i < size; i++) {
		p[i] = trap_get_byte(ctx, parms + i);
		if (p[i] == 10 || p[i] == 13 || p[i] == 0)
			break;
	}
	p[i] = 0;
	parms_in = au (parms_p);
	ret = cfgfile_modify (index, parms_in, size, out_p, outsize);
	xfree (parms_in);
	if (out && outsize > 0) {
		parms_out = ua (out_p);
		if (!trap_valid_address(ctx, out, strlen(parms_out) + 1 > outsize ? outsize : strlen(parms_out) + 1))
			return 0;
		trap_put_string(ctx, parms_out, out, outsize - 1);
	}
	xfree (parms_out);
end:
	xfree (out_p);
	xfree (parms_p);
	return ret;
}

static const TCHAR *cfgfile_read_config_value (const TCHAR *option)
{
	struct strlist *sl;
	for (sl = currprefs.all_lines; sl; sl = sl->next) {
		if (sl->option && !_tcsicmp (sl->option, option))
			return sl->value;
	}
	return NULL;
}

uae_u32 cfgfile_uaelib(TrapContext *ctx, int mode, uae_u32 name, uae_u32 dst, uae_u32 maxlen)
{
	TCHAR *str;
	uae_char tmpa[CONFIG_BLEN];

	if (mode || maxlen > CONFIG_BLEN)
		return 0;

	if (!trap_valid_string(ctx, name, CONFIG_BLEN))
		return 0;
	if (!trap_valid_address(ctx, dst, maxlen))
		return 0;

	trap_get_string(ctx, tmpa, name, sizeof tmpa);
	str = au(tmpa);
	if (str[0] == 0) {
		xfree(str);
		return 0;
	}
	const TCHAR *value = cfgfile_read_config_value(str);
	xfree(str);
	if (value) {
		char *s = ua (value);
		trap_put_string(ctx, s, dst, maxlen);
		xfree (s);
		return dst;
	}
	return 0;
}

#include "sounddep/sound.h"

void copy_prefs(struct uae_prefs *src, struct uae_prefs *dst)
{
	memcpy(dst, src, sizeof(struct uae_prefs));
}

void default_prefs (struct uae_prefs *p, bool reset, int type)
{
  int i;
	int roms[] = { 6, 7, 8, 9, 10, 14, 5, 4, 3, 2, 1, -1 };
  TCHAR zero = 0;
  struct zfile *f;

	reset_inputdevice_config (p, reset);
  memset (p, 0, sizeof (struct uae_prefs));
  _tcscpy (p->description, _T("UAE default configuration"));

  p->start_gui = true;

  p->all_lines = 0;
	p->z3_mapping_mode = Z3MAPPING_AUTO;

	p->mountitems = 0;
	for (i = 0; i < MOUNT_CONFIG_SIZE; i++) {
		p->mountconfig[i].configoffset = -1;
	}

	p->jports[0].id = -1;
	p->jports[1].id = -1;
	p->jports[2].id = -1;
	p->jports[3].id = -1;
	if (reset) {
		inputdevice_joyport_config_store(p, _T("mouse"), 0, -1, 0);
#ifdef PANDORA
		inputdevice_joyport_config_store(p, _T("joy1"), 1, -1, 0);
#else
		inputdevice_joyport_config_store(p, _T("kbd1"), 1, -1, 0);
#endif
	}
	p->keyboard_lang = KBD_LANG_US;

  p->produce_sound = 3;
  p->sound_stereo = SND_STEREO;
  p->sound_stereo_separation = 7;
  p->sound_mixed_stereo_delay = 0;
  p->sound_freq = DEFAULT_SOUND_FREQ;
  p->sound_interpol = 0;
  p->sound_filter = FILTER_SOUND_OFF;
  p->sound_filter_type = 0;
	p->sound_volume_cd = 20;

#if defined(USE_JIT_FPU)
	p->compfpu = 1;
#endif
  p->cachesize = 0;

  p->gfx_framerate = 0;
#if defined(RASPBERRY) || defined(ANDROID)
	p->gfx_monitor.gfx_size.width = 640;
	p->gfx_monitor.gfx_size.height = 262;
  p->gfx_resolution = RES_HIRES;
#else
  p->gfx_monitor.gfx_size.width = 320;
   p->gfx_monitor.gfx_size.height = 240;
  p->gfx_resolution = RES_LORES;
#endif
	p->gfx_vresolution = VRES_NONDOUBLE;

  p->immediate_blits = 0;
	p->waiting_blits = 0;
  p->chipset_refreshrate = 50;
  p->collision_level = 2;
  p->leds_on_screen = 0;
	p->leds_on_screen_mask[0] = p->leds_on_screen_mask[1] = 0x01fe;
	p->scsi = 0;
	p->turbo_emulation = 0;
	p->boot_rom = 0;
#ifdef FAST_COPPER_DEFAULT_ON
  p->fast_copper = 1;
#else
	p->fast_copper = 0;
#endif
	p->cart_internal = 1;

	p->cs_compatible = CP_GENERIC;
	p->cs_rtc = 2;
	p->cs_df0idhw = 1;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = p->cs_cd32fmv = false;
	p->cs_cd32nvram_size = 1024;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_ciatodbug = false;
	p->cs_unmapped_space = 0;
	p->cs_ciatype[0] = 0;
	p->cs_ciatype[1] = 0;

  _tcscpy (p->floppyslots[0].df, _T(""));
  _tcscpy (p->floppyslots[1].df, _T(""));
  _tcscpy (p->floppyslots[2].df, _T(""));
  _tcscpy (p->floppyslots[3].df, _T(""));

	configure_rom (p, roms, 0);
  _tcscpy (p->romextfile, _T(""));
	_tcscpy (p->flashfile, _T(""));
	_tcscpy (p->cartfile, _T(""));

  sprintf (p->path_rom.path[0], _T("%s/kickstarts/"), start_path_data);
  sprintf (p->path_floppy.path[0], _T("%s/disks/"), start_path_data);
  sprintf (p->path_hardfile.path[0], _T("%s/"), start_path_data);
  sprintf (p->path_cd.path[0], _T("%s/cd32/"), start_path_data);

  p->fpu_model = 0;
  p->cpu_model = 68000;
	p->fpu_no_unimplemented = false;
	p->fpu_strict = 0;
  p->m68k_speed = 0;
  p->cpu_compatible = 0;
  p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->cpu_memory_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
  p->chipset_mask = CSMASK_ECS_AGNUS;
  p->ntscmode = 0;
	p->filesys_limit = 0;
	p->filesys_max_name = 107;

  p->fastmem[0].size = 0x00000000;
	p->mbresmem_low.size = 0x00000000;
	p->mbresmem_high.size = 0x00000000;
  p->z3fastmem[0].size = 0x00000000;
  p->z3autoconfig_start = 0x10000000;
  p->chipmem.size = 0x00080000;
  p->bogomem.size = 0x00080000;
	p->rtgboards[0].rtgmem_size = 0x00000000;
	p->rtgboards[0].rtgmem_type = GFXBOARD_UAE_Z3;
	p->custom_memory_addrs[0] = 0;
	p->custom_memory_sizes[0] = 0;
	p->custom_memory_addrs[1] = 0;
	p->custom_memory_sizes[1] = 0;

  p->nr_floppies = 2;
	p->floppy_read_only = false;
  p->floppyslots[0].dfxtype = DRV_35_DD;
  p->floppyslots[1].dfxtype = DRV_35_DD;
  p->floppyslots[2].dfxtype = DRV_NONE;
  p->floppyslots[3].dfxtype = DRV_NONE;
  p->floppy_speed = 100;
  p->floppy_write_length = 0;
	p->dfxclickvolume_disk[0] = 100;
	p->dfxclickvolume_disk[1] = 100;
	p->dfxclickvolume_disk[2] = 100;
	p->dfxclickvolume_disk[3] = 100;
	p->dfxclickvolume_empty[0] = 100;
	p->dfxclickvolume_empty[1] = 100;
	p->dfxclickvolume_empty[2] = 100;
	p->dfxclickvolume_empty[3] = 100;
	p->cd_speed = 100;
  
	p->socket_emu = 0;

  p->input_tablet = TABLET_OFF;

  inputdevice_default_prefs (p);

	blkdev_default_prefs (p);

	struct chipset_refresh *cr;
	cr = &p->cr[CHIPSET_REFRESH_PAL];
	cr->index = CHIPSET_REFRESH_PAL;
	cr->rate = 50.0;
	cr->ntsc = 0;
	cr = &p->cr[CHIPSET_REFRESH_NTSC];
	cr->index = CHIPSET_REFRESH_NTSC;
	cr->rate = 60.0;
	cr->ntsc = 1;

	savestate_state = 0;

  target_default_options (p, type);

	zfile_fclose (default_file);
	default_file = NULL;
	f = zfile_fopen_empty (NULL, _T("configstore"));
	if (f) {
		uaeconfig++;
		cfgfile_save_options (f, p, 0);
		uaeconfig--;
		cfg_write (&zero, f);
		default_file = f;
	}
}

static void buildin_default_prefs_68020 (struct uae_prefs *p)
{
	p->cpu_model = 68020;
	p->address_space_24 = 1;
	p->cpu_compatible = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;
	p->chipmem.size = 0x200000;
	p->bogomem.size = 0;
	p->m68k_speed = -1;
}

static void buildin_default_prefs (struct uae_prefs *p)
{
	p->floppyslots[0].dfxtype = DRV_35_DD;
	if (p->nr_floppies != 1 && p->nr_floppies != 2)
		p->nr_floppies = 2;
	p->floppyslots[1].dfxtype = p->nr_floppies >= 2 ? DRV_35_DD : DRV_NONE;
	p->floppyslots[2].dfxtype = DRV_NONE;
	p->floppyslots[3].dfxtype = DRV_NONE;
	p->floppy_speed = 100;

	p->fpu_model = 0;
	p->cpu_model = 68000;
	p->m68k_speed = 0;
	p->cpu_compatible = 1;
	p->address_space_24 = 1;
	p->cpu_cycle_exact = 0;
	p->cpu_memory_cycle_exact = 0;
	p->blitter_cycle_exact = 0;
	p->chipset_mask = CSMASK_ECS_AGNUS;
	p->immediate_blits = 0;
	p->waiting_blits = 0;
	p->collision_level = 2;
	if (p->produce_sound < 2)
		p->produce_sound = 2;
	p->scsi = 0;
	p->turbo_emulation = 0;
	p->cachesize = 0;
	p->socket_emu = 0;
	p->sound_volume_cd = 0;

	p->chipmem.size = 0x00080000;
	p->bogomem.size = 0x00080000;
	memset(p->fastmem, 0, sizeof(struct ramboard));
	memset(p->z3fastmem, 0, sizeof(struct ramboard));
	p->mbresmem_low.size = 0x00000000;
	p->mbresmem_high.size = 0x00000000;
	p->rtgboards[0].rtgmem_size = 0x00000000;
	p->rtgboards[0].rtgmem_type = GFXBOARD_UAE_Z3;
	for (int i = 0; i < MAX_EXPANSION_BOARDS; i++) {
		memset(&p->expansionboard[i], 0, sizeof(struct boardromconfig));
	}

	p->cs_rtc = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ramseyrev = -1;
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = p->cs_cd32fmv = false;
	p->cs_ide = 0;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_df0idhw = 1;
	p->cs_ciatodbug = false;

	_tcscpy (p->romextfile, _T(""));

	p->mountitems = 0;
  p->leds_on_screen = 0;

	target_default_options (p, 1);
	cfgfile_compatibility_romtype(p);
}

static void set_68020_compa (struct uae_prefs *p, int compa, int cd32)
{
	switch (compa)
	{
	  case 0:
		  p->cpu_compatible = 0;
		  p->m68k_speed = 0;
	    break;
	  case 1:
		  p->cpu_compatible = 0;
		  p->m68k_speed = 0;
		  break;
	  case 2:
		  p->cpu_compatible = 0;
		  p->m68k_speed = -1;
		  p->address_space_24 = 0;
		  break;
	  case 3:
		  p->cpu_compatible = 0;
		  p->address_space_24 = 0;
		  p->cachesize = MAX_JIT_CACHE;
		  break;
	}
	if (p->cpu_model >= 68030)
		p->address_space_24 = 0;
}

/* 0: cycle-exact
* 1: more compatible
* 2: no more compatible, no 100% sound
* 3: no more compatible, waiting blits, no 100% sound
*/

static void set_68000_compa (struct uae_prefs *p, int compa)
{
	switch (compa)
	{
	  case 0:
		  break;
	  case 1:
		  break;
	  case 2:
		  p->cpu_compatible = 0;
		  break;
	  case 3:
		  p->produce_sound = 2;
		  p->cpu_compatible = 0;
		  break;
	}
}

static int bip_a4000 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[8];

	roms[0] = 16;
	roms[1] = 31;
	roms[2] = 13;
	roms[3] = 12;
	roms[4] = -1;

	p->bogomem.size = 0;
	p->chipmem.size = 0x200000;
	p->mbresmem_low.size = 8 * 1024 * 1024;
	p->cpu_model = 68030;
	p->fpu_model = 68882;
	switch (config)
	{
		case 1:
		  p->cpu_model = 68040;
		  p->fpu_model = 68040;
		  break;
	}
	p->chipset_mask = CSMASK_AGA | CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
  p->cpu_compatible = p->address_space_24 = 0;
	p->m68k_speed = -1;
	p->immediate_blits = 0;
	p->produce_sound = 2;
	p->cachesize = MAX_JIT_CACHE;
  p->nr_floppies = 2;
	p->floppyslots[0].dfxtype = DRV_35_HD;
	p->floppyslots[1].dfxtype = DRV_35_HD;
	p->floppy_speed = 0;
	p->cs_compatible = CP_A4000;
	built_in_chipset_prefs (p);
	p->cs_ciaatod = p->ntscmode ? 2 : 1;
	return configure_rom (p, roms, romcheck);
}

static int bip_cd32 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[3];

	buildin_default_prefs_68020 (p);
	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = true;
	p->nr_floppies = 0;
	p->floppyslots[0].dfxtype = DRV_NONE;
	p->floppyslots[1].dfxtype = DRV_NONE;
	p->cs_unmapped_space = 1;
	set_68020_compa (p, compa, 1);
	p->cs_compatible = CP_CD32;
	built_in_chipset_prefs (p);
	fetch_datapath (p->flashfile, sizeof (p->flashfile) / sizeof (TCHAR));
	_tcscat (p->flashfile, _T("cd32.nvr"));
	roms[0] = 64;
	roms[1] = -1;
	if (!configure_rom (p, roms, 0)) {
		roms[0] = 18;
		roms[1] = -1;
		if (!configure_rom (p, roms, romcheck))
			return 0;
		roms[0] = 19;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}
	if (config > 0) {
		p->cs_cd32fmv = true;
		roms[0] = 74;
		roms[1] = 23;
		roms[2] = -1;
		if (!configure_rom (p, roms, romcheck))
			return 0;
	}

	p->cdslots[0].inuse = true;
	p->cdslots[0].type = SCSI_UNIT_IMAGE;
	
	p->gfx_monitor.gfx_size.width = 384;
	p->gfx_monitor.gfx_size.height = 256;

	p->m68k_speed = M68K_SPEED_14MHZ_CYCLES;

	return 1;
}

static int bip_a1200 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	buildin_default_prefs_68020 (p);
	roms[0] = 11;
	roms[1] = 15;
	roms[2] = 31;
	roms[3] = -1;
	p->cs_rtc = 0;
	p->cs_compatible = CP_A1200;
	built_in_chipset_prefs (p);
	switch (config)
	{
		case 1:
		  p->fastmem[0].size = 0x400000;
		  p->cs_rtc = 1;
		  break;
	}
	set_68020_compa (p, compa, 0);

	p->m68k_speed = M68K_SPEED_14MHZ_CYCLES;

  p->nr_floppies = 1;
	p->floppyslots[1].dfxtype = DRV_NONE;

	return configure_rom (p, roms, romcheck);
}

static int bip_a600 (struct uae_prefs *p, int config, int compa, int romcheck)
{
	int roms[4];

	roms[0] = 10;
	roms[1] = 9;
	roms[2] = 8;
	roms[3] = -1;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A600;
	built_in_chipset_prefs (p);
	p->bogomem.size = 0;
	p->chipmem.size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem.size = 0x200000;
	if (config == 2)
		p->fastmem[0].size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
	return configure_rom (p, roms, romcheck);
}

static int bip_a500p (struct uae_prefs *p, int config, int compa, int romcheck)
{
  int roms[2];

	roms[0] = 7;
  roms[1] = -1;
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500P;
	built_in_chipset_prefs (p);
	p->bogomem.size = 0;
  p->chipmem.size = 0x100000;
	if (config > 0)
		p->cs_rtc = 1;
	if (config == 1)
		p->chipmem.size = 0x200000;
	if (config == 2)
		p->fastmem[0].size = 0x400000;
	p->chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;
  return configure_rom (p, roms, romcheck);
}
static int bip_a500 (struct uae_prefs *p, int config, int compa, int romcheck)
{
  int roms[4];

	roms[0] = roms[1] = roms[2] = roms[3] = -1;
	switch (config)
  {
	  case 0: // KS 1.3, OCS Agnus, 0.5M Chip + 0.5M Slow
  	  roms[0] = 6;
		  roms[1] = 32;
		  p->chipset_mask = 0;
		  break;
	  case 1: // KS 1.3, ECS Agnus, 0.5M Chip + 0.5M Slow
		  roms[0] = 6;
		  roms[1] = 32;
		  break;
	  case 2: // KS 1.3, ECS Agnus, 1.0M Chip
		  roms[0] = 6;
		  roms[1] = 32;
		  p->bogomem.size = 0;
		  p->chipmem.size = 0x100000;
		  break;
	  case 3: // KS 1.3, OCS Agnus, 0.5M Chip
		  roms[0] = 6;
		  roms[1] = 32;
		  p->bogomem.size = 0;
		  p->chipset_mask = 0;
		  p->cs_rtc = 0;
		  p->floppyslots[1].dfxtype = DRV_NONE;
		  break;
    case 4: // KS 1.2, OCS Agnus, 0.5M Chip
    	roms[0] = 5;
    	roms[1] = 4;
    	roms[2] = 3;
		  p->bogomem.size = 0;
	    p->chipset_mask = 0;
		  p->cs_rtc = 0;
		  p->floppyslots[1].dfxtype = DRV_NONE;
		  break;
	  case 5: // KS 1.2, OCS Agnus, 0.5M Chip + 0.5M Slow
		  roms[0] = 5;
		  roms[1] = 4;
		  roms[2] = 3;
		  p->chipset_mask = 0;
		  break;
	}
#ifdef FAST_COPPER_DEFAULT_ON
  p->fast_copper = 1;
#else
  p->fast_copper = 0;
#endif
	set_68000_compa (p, compa);
	p->cs_compatible = CP_A500;
	built_in_chipset_prefs (p);
	return configure_rom (p, roms, romcheck);
}

int built_in_prefs (struct uae_prefs *p, int model, int config, int compa, int romcheck)
{
	int v = 0;

	buildin_default_prefs (p);
	switch (model)
  {
	  case 0:
		  v = bip_a500 (p, config, compa, romcheck);
		  break;
	  case 1:
		  v = bip_a500p (p, config, compa, romcheck);
		  break;
	  case 2:
		  v = bip_a600 (p, config, compa, romcheck);
		  break;
	  case 3:
		  v = bip_a1200 (p, config, compa, romcheck);
		  break;
	  case 4:
		  v = bip_a4000 (p, config, compa, romcheck);
		  break;
	  case 5:
		  v = bip_cd32 (p, config, compa, romcheck);
		  break;
  }
	if ((p->cpu_model >= 68020 || !p->cpu_cycle_exact || !p->cpu_memory_cycle_exact) && !p->immediate_blits)
		p->waiting_blits = 1;
	if (p->sound_filter_type == FILTER_SOUND_TYPE_A500 && (p->chipset_mask & CSMASK_AGA))
		p->sound_filter_type = FILTER_SOUND_TYPE_A1200;
	else if (p->sound_filter_type == FILTER_SOUND_TYPE_A1200 && !(p->chipset_mask & CSMASK_AGA))
		p->sound_filter_type = FILTER_SOUND_TYPE_A500;
	cfgfile_compatibility_romtype(p);
	return v;
}

int built_in_chipset_prefs (struct uae_prefs *p)
{
	if (!p->cs_compatible)
		return 1;

	p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = 0;
	p->cs_fatgaryrev = -1;
	p->cs_ide = 0;
	p->cs_ramseyrev = -1;
	p->cs_pcmcia = 0;
	p->cs_ksmirror_e0 = 1;
	p->cs_ksmirror_a8 = 0;
	p->cs_ciaoverlay = 1;
	p->cs_ciaatod = 0;
	p->cs_rtc = 0;
	p->cs_df0idhw = 1;
	p->cs_ciatodbug = false;
	p->cs_z3autoconfig = false;
	p->cs_unmapped_space = 0;
	p->cs_ciatype[0] = p->cs_ciatype[1] = 0;

	switch (p->cs_compatible)
	{
	  case CP_GENERIC: // generic
		  if (p->cpu_model >= 68020) {
			  // big box-like
			  p->cs_rtc = 2;
  			p->cs_fatgaryrev = 0;
      	p->cs_ide = -1;
  			p->cs_ramseyrev = 0x0f;
			  p->cs_unmapped_space = 1;
		  } else if (p->cpu_compatible) {
			  // very A500-like
	      p->cs_df0idhw = 0;
			  if (p->bogomem.size || p->chipmem.size > 0x80000 || p->fastmem[0].size)
				  p->cs_rtc = 1;
  			p->cs_ciatodbug = true;
		  } else {
			  // sort of A500-like
			  p->cs_ide = -1;
			  p->cs_rtc = 1;
		  }
		  break;
	  case CP_CD32: // CD32
		  p->cs_cd32c2p = p->cs_cd32cd = p->cs_cd32nvram = true;
		  p->cs_ksmirror_e0 = 0;
		  p->cs_ksmirror_a8 = 1;
		  p->cs_ciaoverlay = 0;
		  p->cs_unmapped_space = 1;
		  break;
	  case CP_A500: // A500
    	p->cs_df0idhw = 0;
		  if (p->bogomem.size || p->chipmem.size > 0x80000 || p->fastmem[0].size)
			  p->cs_rtc = 1;
			p->cs_ciatodbug = true;
		  break;
	  case CP_A500P: // A500+
		  p->cs_rtc = 1;
			p->cs_ciatodbug = true;
		  break;
	  case CP_A600: // A600
  		p->cs_ide = IDE_A600A1200;
		  p->cs_pcmcia = 1;
		  p->cs_ksmirror_a8 = 1;
		  p->cs_ciaoverlay = 0;
			p->cs_ciatodbug = true;
		p->cs_ciatype[0] = p->cs_ciatype[1] = 1;
		  break;
	  case CP_A1200: // A1200
		  p->cs_ide = IDE_A600A1200;
		  p->cs_pcmcia = 1;
		  p->cs_ksmirror_a8 = 1;
		  p->cs_ciaoverlay = 0;
		  if (p->fastmem[0].size || p->z3fastmem[0].size)
			  p->cs_rtc = 1;
		  break;
	  case CP_A2000: // A2000
		  p->cs_rtc = 1;
		  p->cs_ciaatod = p->ntscmode ? 2 : 1;
			p->cs_ciatodbug = true;
		  p->cs_unmapped_space = 1;
		  break;
	  case CP_A4000: // A4000
		  p->cs_rtc = 2;
  		p->cs_fatgaryrev = 0;
		  p->cs_ramseyrev = 0x0f;
  		p->cs_ide = IDE_A4000;
		  p->cs_ksmirror_a8 = 0;
		  p->cs_ksmirror_e0 = 0;
		  p->cs_z3autoconfig = true;
		  p->cs_unmapped_space = 1;
		  break;
	}
	return 1;
}

void set_config_changed (void)
{
	config_changed = 1;
}

void config_check_vsync (void)
{
	if (config_changed) {
		config_changed++;
		if (config_changed >= 3)
			config_changed = 0;
	}
}

void error_log (const TCHAR *format, ...)
{
	TCHAR buffer[256], *bufp;
	int bufsize = 256;
	va_list parms;

	if (format == NULL) {
		return;
	}

	va_start (parms, format);
	bufp = buffer;
	for (;;) {
		int count = _vsntprintf (bufp, bufsize - 1, format, parms);
		if (count < 0) {
			bufsize *= 10;
			if (bufp != buffer)
				xfree (bufp);
			bufp = xmalloc (TCHAR, bufsize);
			continue;
		}
		break;
	}
	bufp[bufsize - 1] = 0;
	write_log (_T("%s\n"), bufp);
	va_end (parms);

	if (bufp != buffer)
		xfree (bufp);
}

int bip_a4000 (struct uae_prefs *p, int rom)
{
  return bip_a4000(p, 0, 0, 0);
}

int bip_cd32 (struct uae_prefs *p, int rom)
{
  return bip_cd32(p, 0, 0, 0);
}

int bip_a1200 (struct uae_prefs *p, int rom)
{
	int roms[4];

  int v = bip_a1200(p, 0, 0, 0);
	if(rom == 310)
  {
  	roms[0] = 15;
  	roms[1] = 11;
  	roms[2] = 31;
    roms[3] = -1;
    v = configure_rom (p, roms, 0);
  }

	return v;
}

int bip_a500plus (struct uae_prefs *p, int rom)
{
  int roms[4];

  int v = bip_a500p(p, 0, 0, 0);
	if(rom == 130)
  {
  	roms[0] = 6;
  	roms[1] = 5;
  	roms[2] = 4;
    roms[3] = -1;
  }
  else
  {
  	roms[0] = 7;
  	roms[1] = 6;
  	roms[2] = 5;
    roms[3] = -1;
  }
  return configure_rom (p, roms, 0);
}

int bip_a500 (struct uae_prefs *p, int rom)
{
  int roms[4];

  int v = bip_a500(p, 0, 0, 0);
	if(rom == 130)
  {
  	roms[0] = 6;
  	roms[1] = 5;
  	roms[2] = 4;
    roms[3] = -1;
  }
  else
  {
  	roms[0] = 5;
  	roms[1] = 4;
  	roms[2] = 3;
    roms[3] = -1;
  }
  return configure_rom (p, roms, 0);
}

int bip_a2000 (struct uae_prefs *p, int rom)
{
  int roms[4];

	if(rom == 130)
  {
  	roms[0] = 6;
  	roms[1] = 5;
  	roms[2] = 4;
    roms[3] = -1;
  }
  else
  {
  	roms[0] = 5;
  	roms[1] = 4;
  	roms[2] = 3;
    roms[3] = -1;
  }
	p->cs_compatible = CP_A2000;
	built_in_chipset_prefs (p);
  p->chipmem.size = 0x00080000;
  p->bogomem.size = 0x00080000;
	p->chipset_mask = 0;
  p->cpu_compatible = 0;
#ifdef FAST_COPPER_DEFAULT_ON
  p->fast_copper = 1;
#else
  p->fast_copper = 0;
#endif
  p->nr_floppies = 1;
	p->floppyslots[1].dfxtype = DRV_NONE;
  return configure_rom (p, roms, 0);
}
