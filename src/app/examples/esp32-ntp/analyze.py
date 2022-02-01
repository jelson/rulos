#!/usr/bin/env python3

import pandas as pd
import sys
import os
import re

# given a line of text, extra all foo=<numeric val> pairs from it and return a
# dict containing them
def find_pairs(line):
    pairs = re.findall("(\w+)=([\d]+)", line)
    return {kv[0]: int(kv[1]) for kv in pairs}

def parse(filename):
    lines = []
    for line in open(filename):
        if not '[NTP] stats' in line:
            continue
        lines.append(find_pairs(line))
    return pd.DataFrame(lines)

def process_df(df):
    df['py_offset'] = df['epoch_usec'] - df['local_rx_time']

    df['oneway_rtt'] = (df['raw_rtt_usec'] - df['server_delay'])/2
    df['offset_usec_wrtt'] = df['epoch_usec'] - df['oneway_rtt'] - df['local_rx_time']

    df['offset_usec_norm'] = df['offset_usec'] - df['offset_usec'][0]
    df['offset_usec_wrtt_norm'] = df['offset_usec_wrtt'] - df['offset_usec'][0]

    df['since_boot_sec'] = df['local_rx_time'] / 1000000
    print(df)

def process_file(filename):
    df = parse(filename)
    process_df(df)
    ax = df.plot(
        x='since_boot_sec',
        y=['offset_usec_norm', 'offset_usec_wrtt_norm'],
        grid=True,
        figsize=(20, 10))
    plot_fn = filename + ".offsets.png"
    ax.figure.savefig(plot_fn)


if __name__ == "__main__":
    process_file(sys.argv[1])
