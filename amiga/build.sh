#!/bin/sh
# Builds the Amiga tools into ../amigaApps, where they are kept ready to copy
# to an Amiga drive; needs binutils-m68k-linux-gnu
set -e
cd "$(dirname "$0")"
mkdir -p ../amigaApps
for t in AndroidPlay AndroidBrowser AndroidView HostPlay HostBrowser; do
	m68k-linux-gnu-as -m68000 -o $t.o $t.s
	m68k-linux-gnu-objcopy -O binary -j .text $t.o $t.bin
	python3 hunk.py $t.bin ../amigaApps/$t
	rm -f $t.o $t.bin
done

# HostView is HostPlay for pictures, from the same source
m68k-linux-gnu-as -m68000 --defsym HOSTVIEW=1 -o HostView.o HostPlay.s
m68k-linux-gnu-objcopy -O binary -j .text HostView.o HostView.bin
python3 hunk.py HostView.bin ../amigaApps/HostView
rm -f HostView.o HostView.bin

# Libraries need their ROMTag relocated: linked at 0, with the relocations
# kept (-q), which hunk.py turns into a HUNK_RELOC32
mkdir -p ../amigaApps/Libs/MHI
for l in mhiuae; do
	m68k-linux-gnu-as -m68000 -o $l.o $l.s
	m68k-linux-gnu-ld -Ttext=0 -q -o $l.elf $l.o
	m68k-linux-gnu-objcopy -O binary -j .text $l.elf $l.bin
	m68k-linux-gnu-readelf -rW $l.elf | awk '$3 == "R_68K_32" { print $1 }' > $l.rel
	python3 hunk.py $l.bin ../amigaApps/Libs/MHI/$l.library $l.rel
	rm -f $l.o $l.elf $l.bin $l.rel
done
