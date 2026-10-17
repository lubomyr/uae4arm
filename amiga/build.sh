#!/bin/sh
# Builds the Amiga tools; needs binutils-m68k-linux-gnu
set -e
cd "$(dirname "$0")"
for t in AndroidPlay; do
	m68k-linux-gnu-as -m68000 -o $t.o $t.s
	m68k-linux-gnu-objcopy -O binary -j .text $t.o $t.bin
	python3 hunk.py $t.bin $t
	rm -f $t.o $t.bin
done
