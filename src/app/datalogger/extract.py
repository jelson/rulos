#!/usr/bin/env python3

import sys

class Log:
    def _log_found(self, startline, endline, time):
        i = len(self.index)
        time_min = time / 1000.0 / 60.0
        print(f"Found log {i}: lines {startline:8} -{endline:8}, time {time_min:.2f}")
        self.index.append((startline, endline, time))

    def __init__(self, filename):
        self.lines = open(filename, "r", errors="replace").readlines()
        self.index = []
        last_timestamp = None
        last_start = None
        for linenum, line in enumerate(self.lines):
            if not line:
                continue
            fields = line.split(",")
            if len(fields) < 2:
                continue
            timestamp = int(fields[0])
            if not last_timestamp or (timestamp < last_timestamp and timestamp < 1000):
                if last_timestamp:
                    self._log_found(last_start, linenum-1, last_timestamp)
                last_start = linenum
            last_timestamp = timestamp

        self._log_found(last_start, len(self.lines)-1, last_timestamp)

    def extract(self, lognum, channelnum):
        if lognum < 0 or lognum >= len(self.index):
            print(f"log number {lognum} out of range")

        (startline, endline, time) = self.index[lognum]
        for linenum in range(startline, endline+1):
            line = self.lines[linenum]
            fields = line.split(",")
            if len(fields) < 5:
                continue
            if not (fields[1] == "u" or fields[1] == "in"):
                continue
            this_line_channel = int(fields[2])
            if this_line_channel != channelnum:
                continue
            sys.stdout.write(",".join(fields[4:]))

def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} filename [lognum channelnum]")
        sys.exit(1)

    log = Log(sys.argv[1])

    if len(sys.argv) == 4:
        log.extract(int(sys.argv[2]), int(sys.argv[3]))

main()


