ifeq ($(arch),armeabi-v7a)
	CPU_FLAGS += -mfpu=vfp
	DEFS += -DANDROIDSDL -DUSE_SDL -DPANDORA
    DEFS += -DCPU_arm -DARMV6_ASSEMBLY -DARMV6T2 -DARM_HAS_DIV
	ANDROID = 1
	USE_SDL_VERSION = sdl1
	PROFILER_PATH = /storage/sdcard0/profile/
else ifeq ($(arch),arm64-v8a)
	DEFS += -DANDROIDSDL -DUSE_SDL -DPANDORA
    DEFS += -DCPU_AARCH64
	ANDROID = 1
    AARCH64 = 1
	USE_SDL_VERSION = sdl1
	PROFILER_PATH = /storage/sdcard0/profile/
else
	USE_SDL_VERSION = sdl1
endif

NAME   = uae4arm-$(arch)
O      = o
RM     = rm -f
#CXX    = g++
#STRIP  = strip

PROG   = $(NAME)

all: $(PROG)

#DEBUG=1
#TRACER=1

#GEN_PROFILE=1
#USE_PROFILE=1

SDL_CFLAGS = `sdl-config --cflags`

MORE_CFLAGS += -Isrc-$(arch)/osdep -Isrc -Isrc-$(arch)/include -Isrc-$(arch)/archivers
MORE_CFLAGS += -Wno-write-strings -Wno-narrowing
MORE_CFLAGS += -fuse-ld=gold -fdiagnostics-color=auto
MORE_CFLAGS += -falign-functions=16

LDFLAGS +=  -lm -lz -lflac -logg -lpng -lmpg123 -lmpeg2 -lSDL_ttf -lguichan_sdl -lguichan -lxml2 -L/opt/vc/lib 
#LDFLAGS += -ldl -lgcov --coverage

ifndef DEBUG
MORE_CFLAGS += -O2 -ffast-math -pipe
MORE_CFLAGS += -frename-registers
#MORE_CFLAGS += -ftracer
# flags -Ofast and -funroll-loops caused crash on android
else
  MORE_CFLAGS += -g -rdynamic -funwind-tables -mapcs-frame -DDEBUG -Wl,--export-dynamic
  ifdef TRACER
    TRACE_CFLAGS = -DTRACER -finstrument-functions -Wall -rdynamic
  endif
endif

ifdef GEN_PROFILE
MORE_CFLAGS += -fprofile-generate=$(PROFILER_PATH) -fprofile-arcs -fvpt
endif
ifdef USE_PROFILE
MORE_CFLAGS += -fprofile-use -fprofile-correction -fbranch-probabilities -fvpt
endif

ASFLAGS += $(CPU_FLAGS)

CXXFLAGS += $(SDL_CFLAGS) $(CPU_FLAGS) $(DEFS) $(MORE_CFLAGS)
CFLAGS += $(SDL_CFLAGS) $(CPU_FLAGS) $(DEFS) $(MORE_CFLAGS)

MY_CFLAGS  = $(CFLAGS)

OBJS =	\
	src-$(arch)/akiko.o \
	src-$(arch)/ar.o \
	src-$(arch)/aros.rom.o \
	src-$(arch)/audio.o \
	src-$(arch)/autoconf.o \
	src-$(arch)/blitfunc.o \
	src-$(arch)/blittable.o \
	src-$(arch)/blitter.o \
	src-$(arch)/blkdev.o \
	src-$(arch)/blkdev_cdimage.o \
	src-$(arch)/bsdsocket.o \
	src-$(arch)/calc.o \
	src-$(arch)/cd32_fmv.o \
	src-$(arch)/cd32_fmv_genlock.o \
	src-$(arch)/cdrom.o \
	src-$(arch)/cfgfile.o \
	src-$(arch)/cia.o \
	src-$(arch)/crc32.o \
	src-$(arch)/custom.o \
	src-$(arch)/devices.o \
	src-$(arch)/disk.o \
	src-$(arch)/diskutil.o \
	src-$(arch)/dlopen.o \
	src-$(arch)/drawing.o \
	src-$(arch)/events.o \
	src-$(arch)/expansion.o \
	src-$(arch)/fdi2raw.o \
	src-$(arch)/filesys.o \
	src-$(arch)/flashrom.o \
	src-$(arch)/fpp.o \
	src-$(arch)/fsdb.o \
	src-$(arch)/fsdb_unix.o \
	src-$(arch)/fsusage.o \
	src-$(arch)/gayle.o \
	src-$(arch)/gfxboard.o \
	src-$(arch)/gfxutil.o \
	src-$(arch)/hardfile.o \
	src-$(arch)/hrtmon.rom.o \
	src-$(arch)/ide.o \
	src-$(arch)/inputdevice.o \
	src-$(arch)/keybuf.o \
	src-$(arch)/main.o \
	src-$(arch)/memory.o \
	src-$(arch)/native2amiga.o \
	src-$(arch)/rommgr.o \
	src-$(arch)/rtc.o \
	src-$(arch)/savestate.o \
	src-$(arch)/scsi.o \
	src-$(arch)/statusline.o \
	src-$(arch)/traps.o \
	src-$(arch)/uaelib.o \
	src-$(arch)/uaeresource.o \
	src-$(arch)/zfile.o \
	src-$(arch)/zfile_archive.o \
	src-$(arch)/archivers/7z/7zAlloc.o \
	src-$(arch)/archivers/7z/7zBuf.o \
	src-$(arch)/archivers/7z/7zCrc.o \
	src-$(arch)/archivers/7z/7zCrcOpt.o \
	src-$(arch)/archivers/7z/7zDec.o \
	src-$(arch)/archivers/7z/7zIn.o \
	src-$(arch)/archivers/7z/7zStream.o \
	src-$(arch)/archivers/7z/Bcj2.o \
	src-$(arch)/archivers/7z/Bra.o \
	src-$(arch)/archivers/7z/Bra86.o \
	src-$(arch)/archivers/7z/LzmaDec.o \
	src-$(arch)/archivers/7z/Lzma2Dec.o \
	src-$(arch)/archivers/7z/BraIA64.o \
	src-$(arch)/archivers/7z/Delta.o \
	src-$(arch)/archivers/7z/Sha256.o \
	src-$(arch)/archivers/7z/Xz.o \
	src-$(arch)/archivers/7z/XzCrc64.o \
	src-$(arch)/archivers/7z/XzDec.o \
	src-$(arch)/archivers/dms/crc_csum.o \
	src-$(arch)/archivers/dms/getbits.o \
	src-$(arch)/archivers/dms/maketbl.o \
	src-$(arch)/archivers/dms/pfile.o \
	src-$(arch)/archivers/dms/tables.o \
	src-$(arch)/archivers/dms/u_deep.o \
	src-$(arch)/archivers/dms/u_heavy.o \
	src-$(arch)/archivers/dms/u_init.o \
	src-$(arch)/archivers/dms/u_medium.o \
	src-$(arch)/archivers/dms/u_quick.o \
	src-$(arch)/archivers/dms/u_rle.o \
	src-$(arch)/archivers/lha/crcio.o \
	src-$(arch)/archivers/lha/dhuf.o \
	src-$(arch)/archivers/lha/header.o \
	src-$(arch)/archivers/lha/huf.o \
	src-$(arch)/archivers/lha/larc.o \
	src-$(arch)/archivers/lha/lhamaketbl.o \
	src-$(arch)/archivers/lha/lharc.o \
	src-$(arch)/archivers/lha/shuf.o \
	src-$(arch)/archivers/lha/slide.o \
	src-$(arch)/archivers/lha/uae_lha.o \
	src-$(arch)/archivers/lha/util.o \
	src-$(arch)/archivers/lzx/unlzx.o \
	src-$(arch)/archivers/mp2/kjmp2.o \
	src-$(arch)/archivers/wrp/warp.o \
	src-$(arch)/archivers/zip/unzip.o \
	src-$(arch)/machdep/support.o \
	src-$(arch)/osdep/bsdsocket_host.o \
	src-$(arch)/osdep/caps/generic_caps.o \
	src-$(arch)/osdep/cda_play.o \
	src-$(arch)/osdep/charset.o \
	src-$(arch)/osdep/fsdb_host.o \
	src-$(arch)/osdep/keyboard.o \
	src-$(arch)/osdep/mp3decoder.o \
	src-$(arch)/osdep/picasso96.o \
	src-$(arch)/osdep/writelog.o \
	src-$(arch)/osdep/generic_hardfile.o \
	src-$(arch)/osdep/generic_target.o \
	src-$(arch)/osdep/generic_filesys.o \
	src-$(arch)/osdep/generic_gui.o \
	src-$(arch)/osdep/generic_rp9.o \
	src-$(arch)/osdep/generic_mem.o \
	src-$(arch)/osdep/sigsegv_handler.o \
	src-$(arch)/osdep/gui/GenericListModel.o \
	src-$(arch)/osdep/gui/UaeRadioButton.o \
	src-$(arch)/osdep/gui/UaeDropDown.o \
	src-$(arch)/osdep/gui/UaeCheckBox.o \
	src-$(arch)/osdep/gui/UaeListBox.o \
	src-$(arch)/osdep/gui/SelectorEntry.o \
	src-$(arch)/osdep/gui/ShowHelp.o \
	src-$(arch)/osdep/gui/ShowMessage.o \
	src-$(arch)/osdep/gui/SelectFolder.o \
	src-$(arch)/osdep/gui/SelectFile.o \
	src-$(arch)/osdep/gui/CreateFilesysHardfile.o \
	src-$(arch)/osdep/gui/EditFilesysVirtual.o \
	src-$(arch)/osdep/gui/EditFilesysHardfile.o \
	src-$(arch)/osdep/gui/PanelPaths.o \
	src-$(arch)/osdep/gui/PanelQuickstart.o \
	src-$(arch)/osdep/gui/PanelConfig.o \
	src-$(arch)/osdep/gui/PanelCPU.o \
	src-$(arch)/osdep/gui/PanelChipset.o \
	src-$(arch)/osdep/gui/PanelROM.o \
	src-$(arch)/osdep/gui/PanelRAM.o \
	src-$(arch)/osdep/gui/PanelFloppy.o \
	src-$(arch)/osdep/gui/PanelHD.o \
	src-$(arch)/osdep/gui/PanelInput.o \
	src-$(arch)/osdep/gui/PanelDisplay.o \
	src-$(arch)/osdep/gui/PanelSound.o \
	src-$(arch)/osdep/gui/PanelMisc.o \
	src-$(arch)/osdep/gui/PanelSavestate.o \
	src-$(arch)/osdep/gui/main_window.o \
	src-$(arch)/osdep/gui/Navigation.o
	
ifeq ($(ANDROID), 1)
OBJS += src-$(arch)/osdep/gui/androidsdl_event.o
OBJS += src-$(arch)/osdep/gui/PanelOnScreen.o
endif

ifeq ($(USE_SDL_VERSION),sdl1)
OBJS += src-$(arch)/sounddep/sound_sdl.o
OBJS += src-$(arch)/osdep/pandora.o
OBJS += src-$(arch)/osdep/pandora_gfx.o
OBJS += src-$(arch)/osdep/pandora_input.o
OBJS += src-$(arch)/osdep/gui/sdltruetypefont.o
OBJS += src-$(arch)/osdep/gui/PanelGamePortPandora.o
endif

ifdef AARCH64
    OBJS += src-$(arch)/osdep/aarch64_helper.o
else ifeq ($(ANDROID), 1)
	OBJS += src-$(arch)/osdep/arm_helper.o
else
	OBJS += src-$(arch)/osdep/neon_helper.o
endif

OBJS += src-$(arch)/newcpu.o
OBJS += src-$(arch)/newcpu_common.o
OBJS += src-$(arch)/readcpu.o
OBJS += src-$(arch)/cpudefs.o
OBJS += src-$(arch)/cpustbl.o
OBJS += src-$(arch)/cpuemu_0.o
OBJS += src-$(arch)/cpuemu_4.o
OBJS += src-$(arch)/cpuemu_11.o
OBJS += src-$(arch)/cpuemu_40.o
OBJS += src-$(arch)/cpuemu_44.o
OBJS += src-$(arch)/jit/compemu.o
OBJS += src-$(arch)/jit/compstbl.o
OBJS += src-$(arch)/jit/compemu_fpp.o
OBJS += src-$(arch)/jit/compemu_support.o

ifdef TRACER
src-$(arch)/trace.o: src-$(arch)/trace.c
	$(CC) $(MY_CFLAGS) -std=c99 -c src-$(arch)/trace.c -o src-$(arch)/trace.o

OBJS += src-$(arch)/trace.o
endif

.cpp.o:
	$(CXX) $(MY_CFLAGS) $(TRACE_CFLAGS) -c $< -o $@

.cpp.s:
	$(CXX) $(MY_CFLAGS) -S -c $< -o $@

$(PROG): $(OBJS)
	$(CXX) -o $(PROG) $(OBJS) $(LDFLAGS)
ifndef DEBUG
	$(STRIP) $(PROG)
endif

ASMS = \
	src-$(arch)/audio.s \
	src-$(arch)/autoconf.s \
	src-$(arch)/blitfunc.s \
	src-$(arch)/blitter.s \
	src-$(arch)/cia.s \
	src-$(arch)/custom.s \
	src-$(arch)/disk.s \
	src-$(arch)/drawing.s \
	src-$(arch)/events.s \
	src-$(arch)/expansion.s \
	src-$(arch)/filesys.s \
	src-$(arch)/fpp.s \
	src-$(arch)/fsdb.s \
	src-$(arch)/fsdb_unix.s \
	src-$(arch)/fsusage.s \
	src-$(arch)/gfxutil.s \
	src-$(arch)/hardfile.s \
	src-$(arch)/inputdevice.s \
	src-$(arch)/keybuf.s \
	src-$(arch)/main.s \
	src-$(arch)/memory.s \
	src-$(arch)/native2amiga.s \
	src-$(arch)/traps.s \
	src-$(arch)/uaelib.s \
	src-$(arch)/uaeresource.s \
	src-$(arch)/zfile.s \
	src-$(arch)/zfile_archive.s \
	src-$(arch)/machdep/support.s \
	src-$(arch)/osdep/picasso96.s \
	src-$(arch)/osdep/sigsegv_handler.s \
	src-$(arch)/newcpu.s \
	src-$(arch)/newcpu_common.s \
	src-$(arch)/readcpu.s \
	src-$(arch)/cpudefs.s \
	src-$(arch)/cpustbl.s \
	src-$(arch)/cpuemu_0.s \
	src-$(arch)/cpuemu_4.s \
	src-$(arch)/cpuemu_11.s \
	src-$(arch)/jit/compemu.s \
	src-$(arch)/jit/compemu_fpp.s \
	src-$(arch)/jit/compstbl.s \
	src-$(arch)/jit/compemu_support.s

genasm: $(ASMS)


clean:
	$(RM) $(PROG) $(OBJS) $(ASMS)

delasm:
	$(RM) $(ASMS)
	
bootrom:
	od -v -t xC -w8 src-$(arch)/filesys |tail -n +5 | sed -e "s,^.......,," -e "s,[0123456789abcdefABCDEF][0123456789abcdefABCDEF],db(0x&);,g" > src-$(arch)/filesys_bootrom.cpp
	touch src-$(arch)/filesys.cpp
