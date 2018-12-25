 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Target specific stuff, Pandora and RaspberryPi version
  */

#include <SDL.h>

#define TARGET_NAME "pandora"

#define NO_MAIN_IN_MAIN_C

#define OPTIONSFILENAME "uaeconfig"

#ifndef ARMV6T2
#undef USE_JIT_FPU
#endif

#define MAKEBD(x,y,z) ((((x) - 2000) * 10000 + (y)) * 100 + (z))
#define GETBDY(x) ((x) / 1000000 + 2000)
#define GETBDM(x) (((x) - ((x / 10000) * 10000)) / 100)
#define GETBDD(x) ((x) % 100)


#define UAE4ARMDATE MAKEBD(2018, 10, 20)


STATIC_INLINE FILE *uae_tfopen(const TCHAR *path, const TCHAR *mode)
{
  return fopen(path, mode);
}


extern void fix_apmodes(struct uae_prefs *p);
extern int generic_main (int argc, char *argv[]);


#define OFFSET_Y_ADJUST 15

extern int emulating;

extern int z3base_adr;

extern unsigned long time_per_frame;

void run_gui(void);
void init_max_signals(void);
void wait_for_vsync(void);
void reset_sync(void);
unsigned long target_lastsynctime(void);
extern int screen_is_picasso;

void saveAdfDir(void);
void update_display(struct uae_prefs *);
void black_screen_now(void);
void graphics_subshutdown (void);

extern void getcapslock (void);
void keyboard_settrans (void);

#define REMAP_MOUSEBUTTON_LEFT    -1
#define REMAP_MOUSEBUTTON_RIGHT   -2
#define REMAP_JOYBUTTON_ONE       -3
#define REMAP_JOYBUTTON_TWO       -4
#define REMAP_JOY_UP              -5
#define REMAP_JOY_DOWN            -6
#define REMAP_JOY_LEFT            -7
#define REMAP_JOY_RIGHT           -8
#define REMAP_CD32_GREEN          -9
#define REMAP_CD32_YELLOW         -10
#define REMAP_CD32_PLAY           -11
#define REMAP_CD32_FFW            -12
#define REMAP_CD32_RWD            -13

extern void input_closeall(void);
extern int get_sdlkbd (void);
extern int get_sdlmouse (void);

#if defined(RASPBERRY) && !defined(USE_SDL2)
extern void graphics_thread_leave(void);
#endif

extern void free_AmigaMem(void);
extern void alloc_AmigaMem(void);

#ifdef WITH_LOGGING
extern void ShowLiveInfo(char *msg);
#endif

extern void fetch_configurationpath (char *out, int size);
extern void set_configurationpath(char *newpath);
extern void set_rompath(char *newpath);
extern void fetch_rp9path (char *out, int size);
extern void fetch_savestatepath(char *out, int size);
extern void fetch_screenshotpath(char *out, int size);

extern void extractFileName(const char * str,char *buffer);
extern void extractPath(char *str, char *buffer);
extern void removeFileExtension(char *filename);
extern void ReadConfigFileList(void);
extern void RescanROMs(void);
extern void ClearAvailableROMList(void);

#include <vector>
#include <string>
typedef struct {
  char Name[MAX_PATH];
  char Path[MAX_PATH];
  int ROMType;
} AvailableROM;
extern std::vector<AvailableROM*> lstAvailableROMs;

#define MAX_MRU_DISKLIST 40
extern std::vector<std::string> lstMRUDiskList;
extern void AddFileToDiskList(const char *file, int moveToTop);

#define MAX_MRU_CDLIST 10
extern std::vector<std::string> lstMRUCDList;
extern void AddFileToCDList(const char *file, int moveToTop);

#define AMIGAWIDTH_COUNT 6
#define AMIGAHEIGHT_COUNT 6
extern const int amigawidth_values[AMIGAWIDTH_COUNT];
extern const int amigaheight_values[AMIGAHEIGHT_COUNT];

extern void gui_force_rtarea_hdchange(void);
extern void gui_restart(void);
extern bool hardfile_testrdb (const TCHAR *filename);

extern bool host_poweroff;

#ifdef __cplusplus
  extern "C" {
#endif
  void trace_begin (void);
  void trace_end (void);
#ifdef __cplusplus
  }
#endif

STATIC_INLINE int max(int x, int y)
{
    return x > y ? x : y;
}

STATIC_INLINE void atomic_and(volatile uae_atomic *p, uae_u32 v)
{
	__sync_and_and_fetch(p, v);
}
STATIC_INLINE void atomic_or(volatile uae_atomic *p, uae_u32 v)
{
	__sync_or_and_fetch(p, v);
}
STATIC_INLINE uae_atomic atomic_inc(volatile uae_atomic *p)
{
	return __sync_add_and_fetch(p, 1);
}
STATIC_INLINE uae_atomic atomic_dec(volatile uae_atomic *p)
{
	return __sync_sub_and_fetch(p, 1);
}
STATIC_INLINE uae_u32 atomic_bit_test_and_reset(volatile uae_atomic *p, uae_u32 v)
{
  long mask = (1 << v);
  uae_u32 res = __sync_fetch_and_and(p, ~mask);
	return (res && mask);
}
STATIC_INLINE void atomic_set(volatile uae_atomic *p, uae_u32 v)
{
  __sync_lock_test_and_set(p, v);
}

#ifdef USE_JIT_FPU
#ifdef __cplusplus
  extern "C" {
#endif
    void save_host_fp_regs(void* buf);
    void restore_host_fp_regs(void* buf);
#ifdef __cplusplus
  }
#endif
#endif
