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

# Build the whole payload in memory, then write in large chunks.
# Line-by-line writes hit line-buffered TTY flushing on every '\n',
# which caps throughput well below the wire rate.
payload = "".join(f"{PREFIX}{i:0{num_width}d}\n" for i in range(num_lines))

with open(port, "wb", buffering=0) as f:
    data = payload.encode("ascii")
    CHUNK = 4096
    for off in range(0, len(data), CHUNK):
        f.write(data[off:off + CHUNK])
