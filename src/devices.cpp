#include "sysdeps.h"

#include "devices.h"

#include "options.h"
#include "threaddep/thread.h"
#include "memory-uae.h"
#include "audio.h"
#include "scsidev.h"
#include "statusline.h"
#include "cd32_fmv.h"
#include "akiko.h"
#include "disk.h"
#include "cia.h"
#include "inputdevice.h"
#include "blkdev.h"
#include "autoconf.h"
#include "newcpu.h"
#include "savestate.h"
#include "blitter.h"
#include "xwin.h"
#include "custom.h"
#include "serial.h"
#include "bsdsocket.h"
#include "uaeresource.h"
#include "native2amiga.h"
#include "gensound.h"
#include "gui.h"
#include "driveclick.h"
#include "drawing.h"
#ifdef JIT
#include "jit/compemu.h"
#endif

#define MAX_DEVICE_ITEMS 64

static void add_device_item(DEVICE_VOID *pp, int *cnt, DEVICE_VOID p)
{
	for (int i = 0; i < *cnt; i++) {
		if (pp[i] == p)
			return;
	}
	if (*cnt >= MAX_DEVICE_ITEMS) {
		return;
	}
	pp[(*cnt)++] = p;
}
static void execute_device_items(DEVICE_VOID *pp, int cnt)
{
	for (int i = 0; i < cnt; i++) {
		pp[i]();
	}
}

static int device_hsync_cnt;
static DEVICE_VOID device_hsyncs[MAX_DEVICE_ITEMS];
static int device_rethink_cnt;
static DEVICE_VOID device_rethinks[MAX_DEVICE_ITEMS];
static int device_leave_cnt;
static DEVICE_VOID device_leaves[MAX_DEVICE_ITEMS];
static int device_resets_cnt;
static DEVICE_INT device_resets[MAX_DEVICE_ITEMS];
static bool device_reset_done[MAX_DEVICE_ITEMS];

static void reset_device_items(void)
{
	device_hsync_cnt = 0;
	device_rethink_cnt = 0;
	device_resets_cnt = 0;
	device_leave_cnt = 0;
	memset(device_reset_done, 0, sizeof(device_reset_done));
}

void device_add_hsync(DEVICE_VOID p)
{
	add_device_item(device_hsyncs, &device_hsync_cnt, p);
}
void device_add_rethink(DEVICE_VOID p)
{
	add_device_item(device_rethinks, &device_rethink_cnt, p);
}
void device_add_exit(DEVICE_VOID p)
{
	add_device_item(device_leaves, &device_leave_cnt, p);
}
void device_add_reset(DEVICE_INT p)
{
	for (int i = 0; i < device_resets_cnt; i++) {
		if (device_resets[i] == p)
			return;
	}
	device_resets[device_resets_cnt++] = p;
}
void device_add_reset_imm(DEVICE_INT p)
{
	for (int i = 0; i < device_resets_cnt; i++) {
		if (device_resets[i] == p)
			return;
	}
	device_reset_done[device_resets_cnt] = true;
	device_resets[device_resets_cnt++] = p;
	p(1);
}

void device_check_config(void)
{
  check_prefs_changed_audio ();
  check_prefs_changed_custom ();
  check_prefs_changed_cpu ();
	check_prefs_picasso();
}

void devices_reset_ext(int hardreset)
{
	for (int i = 0; i < device_resets_cnt; i++) {
		if (!device_reset_done[i]) {
			device_resets[i](hardreset);
			device_reset_done[i] = true;
		}
	}
}

void devices_reset(int hardreset)
{
	memset(device_reset_done, 0, sizeof(device_reset_done));
	// must be first
	init_eventtab();
	init_shm();
	memory_reset();
	DISK_reset ();
	CIA_reset ();
#ifdef JIT
  compemu_reset ();
#endif
	native2amiga_reset();
#ifdef SCSIEMU
	scsidev_reset();
	scsidev_start_threads();
#endif
#ifdef DRIVESOUND
	driveclick_reset();
#endif
#ifdef FILESYS
	filesys_prepare_reset();
	filesys_reset();
#endif
#if defined (BSDSOCKET)
	bsdlib_reset();
#endif
#ifdef FILESYS
	filesys_start_threads();
	hardfile_reset();
#endif
	device_func_reset();
#ifdef AUTOCONFIG
	rtarea_reset();
#endif
	uae_int_requested = 0;
}

void devices_vsync_pre(void)
{
	blkdev_vsync ();
  CIA_vsync_prehandler();
  inputdevice_vsync ();
  filesys_vsync ();
	statusline_vsync();
}

void devices_hsync(void)
{
	DISK_hsync();
	audio_hsync();

	decide_blitter (-1);
	serial_hsynchandler ();

	execute_device_items(device_hsyncs, device_hsync_cnt);
}

void devices_rethink(void)
{
  rethink_cias ();

	execute_device_items(device_rethinks, device_rethink_cnt);

	rethink_uae_int();
}

void devices_update_sync(double svpos, double syncadjust)
{
	cd32_fmv_set_sync(svpos, syncadjust);
}

void do_leave_program (void)
{
#ifdef JIT
  compiler_exit();
#endif
  graphics_leave ();
  inputdevice_close ();
  DISK_free ();
  close_sound ();
 	gui_exit ();
  hardfile_reset();
#ifdef AUTOCONFIG
  expansion_cleanup ();
#endif
#ifdef FILESYS
  filesys_cleanup ();
#endif
#ifdef BSDSOCKET
  bsdlib_reset ();
#endif
	device_func_free();
  memory_cleanup ();
	free_shm ();
  cfgfile_addcfgparam (0);
  machdep_free ();
	driveclick_free();
	rtarea_free();

	execute_device_items(device_leaves, device_leave_cnt);
}

void virtualdevice_init (void)
{
	reset_device_items();

#ifdef CD32
	akiko_init();
#endif
#ifdef AUTOCONFIG
	rtarea_setup ();
#endif
#ifdef FILESYS
	rtarea_init ();
	uaeres_install ();
	hardfile_install ();
#endif
#ifdef SCSIEMU
	scsidev_install ();
#endif
#ifdef AUTOCONFIG
	expansion_init ();
	emulib_install ();
#endif
#ifdef FILESYS
	filesys_install ();
#endif
#if defined (BSDSOCKET)
	bsdlib_install ();
#endif
}

void devices_restore_start(void)
{
	restore_cia_start ();
	restore_blkdev_start();
  changed_prefs.bogomem.size = 0;
  changed_prefs.chipmem.size = 0;
	changed_prefs.fastmem[0].size = 0;
	changed_prefs.z3fastmem[0].size = 0;
	changed_prefs.mbresmem_low.size = 0;
	changed_prefs.mbresmem_high.size = 0;
	restore_expansion_boards(NULL);
}
