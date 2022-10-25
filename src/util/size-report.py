#!/usr/bin/env python3

import io
import os
import pandas as pd
import re
import subprocess
import sys

def size_report(image):
    syms = subprocess.check_output(
        f"{os.environ['ARM_TOOLCHAIN_DIR']}/arm-none-eabi-objdump -t {image}",
        shell=True)
    lines = [line.decode('utf-8').strip() for line in syms.splitlines()]
    lines = [line for line in lines if line[17:22] in ('.text', '.data')]
    buf = io.StringIO()
    buf.write("\n".join(lines))
    buf.seek(0)
    df = pd.read_fwf(buf, header=None)
    df['size'] = df[4].apply(int, base=16)
    df = df.sort_values('size', ascending=False)
    df['cum'] = df['size'].cumsum()
    pd.set_option('display.max_rows', None)
    pd.set_option('display.max_columns', None)
    pd.set_option('display.expand_frame_repr', False)
    tot = df['size'].sum()
    print(f"{image} total size: {tot}")
    print(df.loc[df['size'] > 0])
    return tot

size_report(sys.argv[1])
