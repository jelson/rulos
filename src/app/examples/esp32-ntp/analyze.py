#!/usr/bin/env python3

import pandas as pd
import sys
import os
import re
import numpy as np

class ParsedLine:
    # given a line of text, extra all foo=<numeric val> pairs from it and return a
    # dict containing them
    def find_pairs(self, line):
        pairs = re.findall("(\w+)=([\d\.]+)", line)
        return {kv[0]: np.double(kv[1]) for kv in pairs}

    def __init__(self, l):
        self.raw_line = l
        self.vals = self.find_pairs(l)

        # try to find the experiment elapsed time, if the line starts
        # with something that looks like '[300.40'
        timesearch = re.search('^\[([\d\.]+\w)', l)
        if timesearch:
            self.vals['exptime_raw'] = float(timesearch.group(1))

class ParsedFile:
    def __init__(self, filename):
        self.filename = filename
        self.parsed_lines = []
        for line in open(filename):
            self.parsed_lines.append(ParsedLine(line))

        # If the lines are timestamped, normalize each one against the
        # first timestamp in the file
        if 'exptime_raw' in self.parsed_lines[0].vals:
            start_time = self.parsed_lines[0].vals['exptime_raw']
            for parsed_line in self.parsed_lines:
                parsed_line.vals['exptime'] = parsed_line.vals['exptime_raw'] - start_time

    def get_dataframe(self, search_string):
        pairs = [pl.vals for pl in self.parsed_lines if search_string in pl.raw_line]
        return pd.DataFrame(pairs)

def ntp_stats(parsed_file):
    df = parsed_file.get_dataframe('[NTP] stats')

    df['py_offset'] = df['epoch_usec'] - df['local_rx_time']

    df['oneway_rtt'] = (df['raw_rtt_usec'] - df['server_delay'])/2
    df['offset_usec_wrtt'] = df['epoch_usec'] - df['oneway_rtt'] - df['local_rx_time']

    df['offset_usec_norm'] = df['offset_usec'] - df['offset_usec'][0]
    df['offset_usec_wrtt_norm'] = df['offset_usec_wrtt'] - df['offset_usec'][0]

    ax = df.plot(
        x='since_boot_sec',
        y=['offset_usec_norm', 'offset_usec_wrtt_norm'],
        grid=True,
        figsize=(20, 10))
    plot_fn = parsed_file.filename + ".offsets.png"
    ax.figure.savefig(plot_fn)

def gpio_stats(parsed_file):
    df = parsed_file.get_dataframe('GPIO: ntp timestamp')
    df['nearest_sec'] = round(df['epoch_time'])
    df['error_usec'] = 1000000 * (df['epoch_time'] - df['nearest_sec'])
    print(df)
    ax = df.plot(
        x='exptime',
        y=['error_usec'],
        grid=True,
        figsize=(20, 10))
    ax.set(
        title='ESP32 time estimate via NTP vs GPS ground truth',
        xlabel='Experiment time (sec)',
        ylabel='Error (usec)'
    )
    plot_fn = parsed_file.filename + ".gps_vs_ntp.png"
    ax.figure.savefig(plot_fn)

if __name__ == "__main__":
    parsed_file = ParsedFile(sys.argv[1])
    #emit_ntp_stats(sys.argv[1])
    gpio_stats(parsed_file)
