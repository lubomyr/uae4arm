#!/usr/bin/env python3
# Wraps a plain 68k binary into an Amiga executable: one code hunk, no
# relocations, so the code has to be PC relative.
import struct, sys

code = open(sys.argv[1], 'rb').read()
code += b'\0' * (-len(code) % 4)
n = len(code) // 4
out = struct.pack('>IIIIII', 0x3F3, 0, 1, 0, 0, n)  # HUNK_HEADER: no names, 1 hunk, sizes
out += struct.pack('>II', 0x3E9, n) + code            # HUNK_CODE
out += struct.pack('>I', 0x3F2)                       # HUNK_END
open(sys.argv[2], 'wb').write(out)
