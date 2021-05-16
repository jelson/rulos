#!/usr/bin/env python3

f = open("/dev/ttyACM1", "w")

import time

for i in range(20):
    f.write(f"hello there number {i}\n")
    f.flush()

