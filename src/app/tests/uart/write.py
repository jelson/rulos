#!/usr/bin/env python3

import sys

if len(sys.argv) != 3:
    sys.exit(f"usage: {sys.argv[0]} <port> <num_lines>")

port = sys.argv[1]
num_lines = int(sys.argv[2])

# Pad each line to a fixed 100 characters so every line has the same
# length — linecounter uses the invariant to detect RX byte drops.
LINE_LEN = 100
PREFIX = "hello there number "
num_width = LINE_LEN - len(PREFIX)

# Stream lines out in 4KB chunks. Binary mode + buffering=0 skips
# Python's text-layer line buffering, which would otherwise flush on
# every '\n' and cap throughput well below wire rate.
CHUNK_LINES = max(1, 4096 // (LINE_LEN + 1))

with open(port, "wb", buffering=0) as f:
    i = 0
    while i < num_lines:
        end = min(i + CHUNK_LINES, num_lines)
        chunk = "".join(f"{PREFIX}{j:0{num_width}d}\n" for j in range(i, end))
        f.write(chunk.encode("ascii"))
        i = end
