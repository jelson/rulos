#!/usr/bin/env python3

import sys
import time

f = open(sys.argv[1], "w")

s = ""

for i in range(20):
    s += f"hello there number {i}\n"

f.write(s)
f.flush()

