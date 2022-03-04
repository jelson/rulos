#!/usr/bin/env python3

import sys
import time

f = open(sys.argv[1], "w")

for i in range(20):
    f.write(f"hello there number {i}//")
    f.flush()

