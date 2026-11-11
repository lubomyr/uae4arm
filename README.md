# uae4arm for Android

![uae4arm](banner.png)

A fast Amiga emulator for Android, based on TomB's UAE4ARM
with large parts brought over from WinUAE. It runs from a plain A500 with
floppies up to an A4000 with a 68060, Picasso96 graphics, AHI sound and
networking.

Download the APK from [Releases](https://github.com/lubomyr/uae4arm/releases).
Kickstart ROMs are not included; without them the built-in AROS ROM is used.

## Features

**Emulation**
- 68000, 68010, 68020, 68030, 68040 and 68060 CPUs, 68881/68882 and internal FPUs
- JIT for both 32-bit (armeabi-v7a) and 64-bit (arm64-v8a) devices
- OCS, ECS and AGA chipsets; CD32 and CDTV
- Chip, Slow, Fast and Zorro III Fast RAM (up to 512 MB)
- Picasso96 RTG graphics, 8/16/32-bit modes
- ADF, ADZ, DMS, IPF (via capsimage) floppies; hardfiles with RDB; folders as drives;
  CD images; WHDLoad
- AHI, UAE audio and Toccata sound, MIDI through Android's MIDI devices (software synthesizers, USB)
- bsdsocket.library networking
- Save states

**On Android**
- On-screen joystick and buttons, gamepads, physical keyboards, mouse from the touch screen
- Second screen over HDMI / USB-C (Misc → Use External Display)
- `mhiuae.library`: MP3 decoding on the host for AmigaAMP, HippoPlayer and other MHI players
- QuarkTex NG through `uaenative.library`: Warp3D, MiniGL and ogles2 drawn with the device's GPU
- Host tools in `amigaApps/` for the Amiga side:
  - **HostPlay / HostVideoPlay** — plays a video file in a native Amiga window, decoded by MediaCodec
  - **HostView / HostImageView** — shows images (including IFF ILBM) in a native Amiga window
  - **AndroidPlay, AndroidView, AndroidBrowser** — open a video, picture or web page
    in a floating Android window over the emulator
- Light and dark menu themes

## Building

uae4arm is built inside pelya's libSDL environment for Android
([commandergenius](https://github.com/pelya/commandergenius)) with a patch to it
kept here as `androidsdl.patch`. Step-by-step instructions are in
[How_to_build.txt](How_to_build.txt).

`./LinuxBuild.sh` builds a version for a Linux PC (no JIT, no Android wrapper),
handy for trying emulation changes quickly.

## Credits

Based on the work of many people on UAE and its ports:
- Toni Wilen — WinUAE
- TomB — UAE4ARM and its ARM JIT, 32- and 64-bit
- Dimitris Panokostas (midwan) — amiberry, whose fixes to the JIT were brought over
- Chui — UAE4ALL
- Notaz, Pickle, john4p, tuki_cat — GP2X, Wiz and Pandora ports
- all people from the ARAnyM project for the ARM JIT
- Sdursun — QuarkTex NG
- Frode Solheim — capsimg

## License

GNU General Public License, version 2. See [LICENSE](LICENSE).
