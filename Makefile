ifeq ($(PLATFORM),)
	PLATFORM = android
endif

ifeq ($(PLATFORM),android)
	CPU_FLAGS += -mfpu=neon -mfloat-abi=soft
	DEFS += -DANDROIDSDL
	ANDROID = 1
	HAVE_NEON = 1
	HAVE_SDL_DISPLAY = 1
	USE_PICASSO96 = 1
else ifeq ($(PLATFORM),rpi2)
	CPU_FLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	LDFLAGS += -lbcm_host
	DEFS += -DRASPBERRY -DSIX_AXIS_WORKAROUND
	HAVE_NEON = 1
	HAVE_DISPMANX = 1
	USE_PICASSO96 = 1
else ifeq ($(PLATFORM),rpi1)
	CPU_FLAGS += -mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard
	LDFLAGS += -lbcm_host
	HAVE_DISPMANX = 1
	DEFS += -DRASPBERRY -DSIX_AXIS_WORKAROUND
else ifeq ($(PLATFORM),generic-sdl)
	HAVE_SDL_DISPLAY = 1
	HAVE_NEON = 1
	USE_PICASSO96 = 1
else ifeq ($(PLATFORM),gles)
	HAVE_GLES_DISPLAY = 1
	HAVE_NEON = 1
endif

NAME   = uae4arm
O      = o
RM     = rm -f
#CXX    = g++
#STRIP  = strip

PROG   = $(NAME)

all: $(PROG)

PANDORA=1

#USE_XFD=1

SDL_CFLAGS = `sdl-config --cflags`

DEFS += -DCPU_arm -DARM_ASSEMBLY -DARMV6_ASSEMBLY -DARMV6T2 -DGP2X -DPANDORA -DWITH_INGAME_WARNING
DEFS += -DROM_PATH_PREFIX=\"./\" -DDATA_PREFIX=\"./data/\" -DSAVE_PREFIX=\"./saves/\"
DEFS += -DUSE_SDL

ifeq ($(USE_PICASSO96), 1)
	DEFS += -DPICASSO96
endif

ifeq ($(HAVE_NEON), 1)
	DEFS += -DUSE_ARMNEON
endif

MORE_CFLAGS += -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads

MORE_CFLAGS += -Isrc/osdep -Isrc -Isrc/include -Isrc/od-pandora -fomit-frame-pointer -Wno-unused -Wno-format -Wno-write-strings -Wno-multichar -DUSE_SDL
MORE_CFLAGS += -fexceptions -fpermissive

LDFLAGS +=  -lm -lz -lflac -logg -lpng -lmpg123 -lSDL_ttf -lguichan_sdl -lguichan -lxml2 -L/opt/vc/lib 

ifndef DEBUG
MORE_CFLAGS += -Ofast -pipe -ftree-vectorize -fsingle-precision-constant -fuse-ld=gold -fdiagnostics-color=auto
MORE_CFLAGS += -mstructure-size-boundary=32
MORE_CFLAGS += -falign-functions=32
MORE_CFLAGS += -fno-builtin -fweb -frename-registers
MORE_CFLAGS += -fipa-pta
else
MORE_CFLAGS += -ggdb
endif

ASFLAGS += $(CPU_FLAGS)

CXXFLAGS += $(SDL_CFLAGS) $(CPU_FLAGS) $(DEFS) $(MORE_CFLAGS)
CFLAGS += $(SDL_CFLAGS) $(CPU_FLAGS) $(DEFS) $(MORE_CFLAGS)

OBJS =	\
	src/akiko.o \
	src/aros.rom.o \
	src/audio.o \
	src/autoconf.o \
	src/blitfunc.o \
	src/blittable.o \
	src/blitter.o \
	src/blkdev.o \
	src/blkdev_cdimage.o \
	src/bsdsocket.o \
	src/cdrom.o \
	src/cfgfile.o \
	src/cia.o \
	src/crc32.o \
	src/custom.o \
	src/disk.o \
	src/diskutil.o \
	src/drawing.o \
	src/events.o \
	src/expansion.o \
	src/filesys.o \
	src/fpp.o \
	src/fsdb.o \
	src/fsdb_unix.o \
	src/fsusage.o \
	src/gfxutil.o \
	src/hardfile.o \
	src/inputdevice.o \
	src/keybuf.o \
	src/main.o \
	src/memory.o \
	src/native2amiga.o \
	src/rommgr.o \
	src/savestate.o \
	src/traps.o \
	src/uaelib.o \
	src/uaeresource.o \
	src/zfile.o \
	src/zfile_archive.o \
	src/archivers/7z/Archive/7z/7zAlloc.o \
	src/archivers/7z/Archive/7z/7zDecode.o \
	src/archivers/7z/Archive/7z/7zExtract.o \
	src/archivers/7z/Archive/7z/7zHeader.o \
	src/archivers/7z/Archive/7z/7zIn.o \
	src/archivers/7z/Archive/7z/7zItem.o \
	src/archivers/7z/7zBuf.o \
	src/archivers/7z/7zCrc.o \
	src/archivers/7z/7zStream.o \
        src/archivers/7z/Bcj2.o \
	src/archivers/7z/Bra.o \
	src/archivers/7z/Bra86.o \
	src/archivers/7z/LzmaDec.o \
	src/archivers/dms/crc_csum.o \
	src/archivers/dms/getbits.o \
	src/archivers/dms/maketbl.o \
	src/archivers/dms/pfile.o \
	src/archivers/dms/tables.o \
	src/archivers/dms/u_deep.o \
	src/archivers/dms/u_heavy.o \
	src/archivers/dms/u_init.o \
	src/archivers/dms/u_medium.o \
	src/archivers/dms/u_quick.o \
	src/archivers/dms/u_rle.o \
	src/archivers/lha/crcio.o \
	src/archivers/lha/dhuf.o \
	src/archivers/lha/header.o \
	src/archivers/lha/huf.o \
	src/archivers/lha/larc.o \
	src/archivers/lha/lhamaketbl.o \
	src/archivers/lha/lharc.o \
	src/archivers/lha/shuf.o \
	src/archivers/lha/slide.o \
	src/archivers/lha/uae_lha.o \
	src/archivers/lha/util.o \
	src/archivers/lzx/unlzx.o \
	src/archivers/wrp/warp.o \
	src/archivers/zip/unzip.o \
	src/machdep/support.o \
	src/osdep/bsdsocket_host.o \
	src/osdep/cda_play.o \
	src/osdep/fsdb_host.o \
	src/osdep/hardfile_pandora.o \
	src/osdep/keyboard.o \
	src/osdep/mp3decoder.o \
	src/osdep/writelog.o \
	src/osdep/pandora.o \
	src/osdep/pandora_filesys.o \
	src/osdep/pandora_input.o \
	src/osdep/pandora_gui.o \
	src/osdep/pandora_rp9.o \
	src/osdep/pandora_mem.o \
	src/osdep/sigsegv_handler.o \
	src/osdep/menu/menu_config.o \
	src/sounddep/sound.o \
	src/osdep/gui/UaeRadioButton.o \
	src/osdep/gui/UaeDropDown.o \
	src/osdep/gui/UaeCheckBox.o \
	src/osdep/gui/UaeListBox.o \
	src/osdep/gui/InGameMessage.o \
	src/osdep/gui/SelectorEntry.o \
	src/osdep/gui/ShowMessage.o \
	src/osdep/gui/SelectFolder.o \
	src/osdep/gui/SelectFile.o \
	src/osdep/gui/CreateFilesysHardfile.o \
	src/osdep/gui/EditFilesysVirtual.o \
	src/osdep/gui/EditFilesysHardfile.o \
	src/osdep/gui/PanelPaths.o \
	src/osdep/gui/PanelConfig.o \
	src/osdep/gui/PanelCPU.o \
	src/osdep/gui/PanelChipset.o \
	src/osdep/gui/PanelROM.o \
	src/osdep/gui/PanelRAM.o \
	src/osdep/gui/PanelFloppy.o \
	src/osdep/gui/PanelHD.o \
	src/osdep/gui/PanelDisplay.o \
	src/osdep/gui/PanelSound.o \
	src/osdep/gui/PanelInput.o \
	src/osdep/gui/PanelMisc.o \
	src/osdep/gui/PanelSavestate.o \
	src/osdep/gui/main_window.o \
	src/osdep/gui/Navigation.o
	
ifeq ($(ANDROID), 1)
OBJS += src/osdep/gui/PanelOnScreen.o
endif

ifeq ($(HAVE_DISPMANX), 1)
OBJS += src/od-rasp/rasp_gfx.o
endif

ifeq ($(HAVE_SDL_DISPLAY), 1)
OBJS += src/osdep/pandora_gfx.o
endif

ifeq ($(HAVE_GLES_DISPLAY), 1)
OBJS += src/od-gles/gl.o
OBJS += src/od-gles/gl_platform.o
OBJS += src/od-gles/gles_gfx.o
MORE_CFLAGS += -I/opt/vc/include/
MORE_CFLAGS += -DHAVE_GLES
LDFLAGS +=  -ldl -lEGL -lGLESv1_CM
endif


ifdef PANDORA
OBJS += src/osdep/gui/sdltruetypefont.o
endif

ifeq ($(USE_PICASSO96), 1)
	OBJS += src/osdep/picasso96.o
endif

ifeq ($(HAVE_NEON), 1)
	OBJS += src/osdep/neon_helper.o
endif

OBJS += src/newcpu.o
OBJS += src/readcpu.o
OBJS += src/cpudefs.o
OBJS += src/cpustbl.o
OBJS += src/cpuemu_0.o
OBJS += src/cpuemu_4.o
OBJS += src/cpuemu_11.o
OBJS += src/jit/compemu.o
OBJS += src/jit/compemu_fpp.o
OBJS += src/jit/compstbl.o
OBJS += src/jit/compemu_support.o

src/osdep/neon_helper.o: src/osdep/neon_helper.s
	$(CXX) $(CPU_FLAGS) -falign-functions=32 -mcpu=cortex-a8 -Wall -o src/osdep/neon_helper.o -c src/osdep/neon_helper.s

$(PROG): $(OBJS)
	$(CXX) -o $(PROG) $(OBJS) $(LDFLAGS)
ifndef DEBUG
	$(STRIP) $(PROG)
endif

clean:
	$(RM) $(PROG) $(OBJS)
