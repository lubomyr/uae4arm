#!/bin/sh
# Builds the Amiga tools into ../amigaApps, where they are kept ready to copy
# to an Amiga drive; needs binutils-m68k-linux-gnu
set -e
cd "$(dirname "$0")"
mkdir -p ../amigaApps
for t in AndroidPlay AndroidBrowser; do
	m68k-linux-gnu-as -m68000 -o $t.o $t.s
	m68k-linux-gnu-objcopy -O binary -j .text $t.o $t.bin
	python3 hunk.py $t.bin ../amigaApps/$t
	rm -f $t.o $t.bin
done
