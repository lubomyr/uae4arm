/*
* UAE - The Un*x Amiga Emulator
*
* Drive Click Emulation Support Functions
*
* Copyright 2004 James Bagg, Toni Wilen
*/

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef DRIVESOUND

#include "uae.h"
#include "options.h"
#include "audio.h"
#include "sounddep/sound.h"
#include "zfile.h"
#include "fsdb.h"
#include "driveclick.h"

static struct drvsample drvs[4][DS_END];
static int freq = 44100;

static int drv_starting[4], drv_spinning[4], drv_has_spun[4], drv_has_disk[4];

static int click_initialized, wave_initialized;
#define DS_SHIFT 10
static int sample_step;
static uae_s16 *clickbuffer;
static int clickcnt;

uae_s16 *decodewav (uae_u8 *s, int *lenp)
{
	uae_s16 *dst;
	uae_u8 *src = s;
	int len;

	if (memcmp (s, "RIFF", 4))
		return 0;
	if (memcmp (s + 8, "WAVE", 4))
		return 0;
	s += 12;
	len = *lenp;
	while (s < src + len) {
		if (!memcmp (s, "fmt ", 4))
			freq = s[8 + 4] | (s[8 + 5] << 8);
		if (!memcmp (s, "data", 4)) {
			s += 4;
			len = s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);
			dst = xmalloc (uae_s16, len / 2);
			memcpy (dst, s + 4, len);
			*lenp = len / 2;
			return dst;
		}
		s += 8 + (s[4] | (s[5] << 8) | (s[6] << 16) | (s[7] << 24));
	}
	return 0;
}

static int loadsample (TCHAR *path, struct drvsample *ds)
{
	struct zfile *f;
	uae_u8 *buf;
	int size;
	TCHAR name[MAX_DPATH];

	f = zfile_fopen (path, _T("rb"), ZFD_NORMAL);
	if (!f) {
		_tcscpy (name, path);
		_tcscat (name, _T(".wav"));
		f = zfile_fopen (name, _T("rb"), ZFD_NORMAL);
		if (!f) {
			write_log (_T("driveclick: can't open '%s' (or '%s')\n"), path, name);
			return 0;
		}
	}
	zfile_fseek (f, 0, SEEK_END);
	size = zfile_ftell (f);
	buf = xmalloc (uae_u8, size);
	zfile_fseek (f, 0, SEEK_SET);
	zfile_fread (buf, size, 1, f);
	zfile_fclose (f);
	ds->len = size;
	ds->p = decodewav (buf, &ds->len);
	xfree (buf);
	return 1;
}

static void freesample (struct drvsample *s)
{
	xfree (s->p);
	s->p = 0;
}

static void driveclick_close(void)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < DS_END; j++)
			freesample (&drvs[i][j]);
	}
	memset (drvs, 0, sizeof (drvs));
	click_initialized = 0;
	wave_initialized = 0;
	driveclick_reset ();
}

void driveclick_free(void)
{
	driveclick_close();
}

void driveclick_init (void)
{
	int v, vv;

	driveclick_close();
	vv = 0;
	for (int i = 0; i < 4; i++) {
		struct floppyslot *fs = &currprefs.floppyslots[i];
		for (int j = 0; j < CLICK_TRACKS; j++)  {
			drvs[i][DS_CLICK].indexes[j] = 0;
			drvs[i][DS_CLICK].lengths[j] = 0;
		}
		if (fs->dfxclick) {
			v = 0;
			if (fs->dfxclick > 0) {
				switch(fs->dfxclick)
				{
				case 1:
					if (driveclick_loadresource (drvs[i], fs->dfxclick))
						v = 3;
					for (int j = 0; j < CLICK_TRACKS; j++)
						drvs[i][DS_CLICK].lengths[j] = drvs[i][DS_CLICK].len;
					wave_initialized = 1;
					break;
				default:
					break;
				}
			}
			if (v == 0) {
				for (int j = 0; j < DS_END; j++)
					freesample (&drvs[i][j]);
				fs->dfxclick = changed_prefs.floppyslots[i].dfxclick = 0;
			} else {
				vv++;
			}
			for (int j = 0; j < DS_END; j++)
				drvs[i][j].len <<= DS_SHIFT;
			drvs[i][DS_CLICK].pos = drvs[i][DS_CLICK].len;
			drvs[i][DS_SNATCH].pos = drvs[i][DS_SNATCH].len;
		}
	}
	if (vv > 0) {
		click_initialized = 1;
	}
	driveclick_reset();
}

void driveclick_reset (void)
{
	xfree (clickbuffer);
	clickbuffer = NULL;
	clickcnt = 0;
	for (int i = 0; i < 4; i++) {
		drv_starting[i] = 0;
		drv_spinning[i] = 0;
		drv_has_spun[i] = 0;
		drv_has_disk[i] = 0;
		if (currprefs.floppyslots[i].df[0])
			driveclick_insert(i, 0);
	}
	if (!wave_initialized)
		return;
	clickbuffer = xcalloc (uae_s16, currprefs.sound_stereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN);
	sample_step = (freq << DS_SHIFT) / currprefs.sound_freq;
}

static int driveclick_active (void)
{
	for (int i = 0; i < 4; i++) {
		if (currprefs.floppyslots[i].dfxclick) {
			if (drv_spinning[i] || drv_starting[i])
				return 1;
		}
	}
	return 0;
}

static uae_s16 getsample (void)
{
	uae_s32 total_sample = 0;
	int total_div = 0;

	for (int i = 0; i < 4; i++) {
		int div = 0;
		if (currprefs.floppyslots[i].dfxclick) {
			uae_s32 smp = 0;
			struct drvsample *ds_start = &drvs[i][DS_START];
			struct drvsample *ds_spin = drv_has_disk[i] ? &drvs[i][DS_SPIN] : &drvs[i][DS_SPINND];
			struct drvsample *ds_click = &drvs[i][DS_CLICK];
			struct drvsample *ds_snatch = &drvs[i][DS_SNATCH];
			if (drv_spinning[i] || drv_starting[i]) {
				div++;
				if (drv_starting[i] && drv_has_spun[i]) {
					if (ds_start->p && ds_start->pos < ds_start->len) {
						smp = ds_start->p[ds_start->pos >> DS_SHIFT];
						ds_start->pos += sample_step;
					} else {
						drv_starting[i] = 0;
					}
				} else if (drv_starting[i] && drv_has_spun[i] == 0) {
					if (ds_snatch->p && ds_snatch->pos < ds_snatch->len) {
						smp = ds_snatch->p[ds_snatch->pos >> DS_SHIFT];
						ds_snatch->pos += sample_step;
					} else {
						drv_starting[i] = 0;
						ds_start->pos = ds_start->len;
						drv_has_spun[i] = 1;
					}
				}
				if (ds_spin->p && drv_starting[i] == 0) {
					if (ds_spin->pos >= ds_spin->len)
						ds_spin->pos -= ds_spin->len;
					smp = ds_spin->p[ds_spin->pos >> DS_SHIFT];
					ds_spin->pos += sample_step;
				}
			}
			if (ds_click->p && ds_click->pos < ds_click->len) {
				smp += ds_click->p[ds_click->pos >> DS_SHIFT];
				div++;
				ds_click->pos += sample_step;
			}
			if (div) {
				int vol;
				if (drv_has_disk[i])
					vol = currprefs.dfxclickvolume_disk[i];
				else
					vol = currprefs.dfxclickvolume_empty[i];
				total_sample += (smp * (100 - vol) / 100) / div;
				total_div++;
			}
		}
	}
	if (!total_div)
		return 0;
	return total_sample / total_div;
}

static void mix (void)
{
	int total = currprefs.sound_stereo ? SNDBUFFER_LEN * 2 : SNDBUFFER_LEN;
	while (clickcnt < total) {
		clickbuffer[clickcnt++] = getsample ();
	}
}

STATIC_INLINE uae_s16 limit (uae_s32 v)
{
	if (v < -32768)
		v = -32768;
	if (v > 32767)
		v = 32767;
	return v;
}

void driveclick_mix (uae_s16 *sndbuffer, int size)
{
	if (!wave_initialized)
		return;
	mix ();
	clickcnt = 0;
	switch (get_audio_nativechannels (currprefs.sound_stereo))
	{
	case 2:
		for (int i = 0; i < size / 2; i++) {
			uae_s16 s = clickbuffer[i];
			sndbuffer[0] = limit (((sndbuffer[0] + s) * 2) / 3);
			sndbuffer[1] = limit (((sndbuffer[1] + s) * 2) / 3);
			sndbuffer += 2;
		}
		break;
	case 1:
		for (int i = 0; i < size; i++) {
			uae_s16 s = clickbuffer[i];
			sndbuffer[0] = limit (((sndbuffer[0] + s) * 2) / 3);
			sndbuffer++;
		}
		break;
	}
}

static void dr_audio_activate (void)
{
	if (audio_activate ())
		clickcnt = 0;
}

void driveclick_click (int drive, int cyl)
{
	static int prevcyl[4];

	if (!click_initialized)
		return;
	if (!currprefs.floppyslots[drive].dfxclick)
		return;
	if (prevcyl[drive] == 0 && cyl == 0) // "noclick" check
		return;
	dr_audio_activate ();
	prevcyl[drive] = cyl;
	if (!wave_initialized) {
		return;
	}
	mix ();
	drvs[drive][DS_CLICK].pos = drvs[drive][DS_CLICK].indexes[cyl] << DS_SHIFT;
	drvs[drive][DS_CLICK].len = (drvs[drive][DS_CLICK].indexes[cyl] + (drvs[drive][DS_CLICK].lengths[cyl] / 2)) << DS_SHIFT;
}

void driveclick_motor (int drive, int running)
{
	if (!click_initialized)
		return;
	if (!currprefs.floppyslots[drive].dfxclick)
		return;
	if (!wave_initialized) {
		return;
	}
	mix ();
	if (running == 0) {
		drv_starting[drive] = 0;
		drv_spinning[drive] = 0;
	} else {
		if (drv_spinning[drive] == 0) {
			dr_audio_activate();
			drv_starting[drive] = 1;
			drv_spinning[drive] = 1;
			if (drv_has_disk[drive] && drv_has_spun[drive] == 0 && drvs[drive][DS_SNATCH].pos >= drvs[drive][DS_SNATCH].len)
				drvs[drive][DS_SNATCH].pos = 0;
			if (running == 2)
				drvs[drive][DS_START].pos = 0;
			drvs[drive][DS_SPIN].pos = 0;
		}
	}
}

void driveclick_insert (int drive, int eject)
{
	if (!click_initialized)
		return;
	if (!wave_initialized)
		return;
	if (!currprefs.floppyslots[drive].dfxclick)
		return;
	if (eject)
		drv_has_spun[drive] = 0;
	if (drv_has_disk[drive] == 0 && !eject)
		dr_audio_activate ();
	drv_has_disk[drive] = !eject;
}

void driveclick_check_prefs (void)
{
	if (!config_changed)
		return;
	if (driveclick_active ())
		dr_audio_activate ();
	if (
		currprefs.dfxclickvolume_disk[0] != changed_prefs.dfxclickvolume_disk[0] ||
		currprefs.dfxclickvolume_disk[1] != changed_prefs.dfxclickvolume_disk[1] ||
		currprefs.dfxclickvolume_disk[2] != changed_prefs.dfxclickvolume_disk[2] ||
		currprefs.dfxclickvolume_disk[3] != changed_prefs.dfxclickvolume_disk[3] ||
		currprefs.dfxclickvolume_empty[0] != changed_prefs.dfxclickvolume_empty[0] ||
		currprefs.dfxclickvolume_empty[1] != changed_prefs.dfxclickvolume_empty[1] ||
		currprefs.dfxclickvolume_empty[2] != changed_prefs.dfxclickvolume_empty[2] ||
		currprefs.dfxclickvolume_empty[3] != changed_prefs.dfxclickvolume_empty[3] ||
		currprefs.floppyslots[0].dfxclick != changed_prefs.floppyslots[0].dfxclick ||
		currprefs.floppyslots[1].dfxclick != changed_prefs.floppyslots[1].dfxclick ||
		currprefs.floppyslots[2].dfxclick != changed_prefs.floppyslots[2].dfxclick ||
		currprefs.floppyslots[3].dfxclick != changed_prefs.floppyslots[3].dfxclick)
	{
		for (int i = 0; i < 4; i++) {
			currprefs.floppyslots[i].dfxclick = changed_prefs.floppyslots[i].dfxclick;
			currprefs.dfxclickvolume_empty[i] = changed_prefs.dfxclickvolume_empty[i];
			currprefs.dfxclickvolume_disk[i] = changed_prefs.dfxclickvolume_disk[i];
		}
		driveclick_init ();
	}
}

#endif
