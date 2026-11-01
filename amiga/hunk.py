#!/usr/bin/env python3
# Wraps a plain 68k binary into an Amiga executable: one code hunk. Without a
# third argument there are no relocations, so the code has to be PC relative;
# with one, it names a file of the offsets of the longwords to relocate, one
# hex number a line, for a binary linked at 0.
import struct, sys

code = open(sys.argv[1], 'rb').read()
code += b'\0' * (-len(code) % 4)
n = len(code) // 4
relocs = []
if len(sys.argv) > 3:
    relocs = [int(l, 16) for l in open(sys.argv[3]) if l.strip()]
out = struct.pack('>IIIIII', 0x3F3, 0, 1, 0, 0, n)  # HUNK_HEADER: no names, 1 hunk, sizes
out += struct.pack('>II', 0x3E9, n) + code            # HUNK_CODE
if relocs:
    out += struct.pack('>III', 0x3EC, len(relocs), 0)  # HUNK_RELOC32, against hunk 0
    out += b''.join(struct.pack('>I', r) for r in relocs)
    out += struct.pack('>I', 0)
out += struct.pack('>I', 0x3F2)                       # HUNK_END
open(sys.argv[2], 'wb').write(out)
