#!/usr/bin/env python3

import sys
import datetime

class Log:
    def _log_found(self, startline, endline, duration, utc_start):
        i = len(self.index)
        duration_min = duration / 1000.0 / 60.0
        if utc_start:
            when = "@" + utc_start.astimezone().isoformat()
        else:
            when = "// no fix"
        sys.stderr.write(f"Found log {i:2}: lines {startline:8} -{endline:8}, {duration_min:6.1f}min {when}\n")
        self.index.append((startline, endline, duration, utc_start))

    def __init__(self, filename):
        self.lines = open(filename, "r", errors="replace").readlines()
        self.index = []
        last_timestamp = None
        last_start = None
        start_date = None
        for linenum, line in enumerate(self.lines):
            if not line:
                continue
            fields = line.split(",")
            if len(fields) < 2:
                continue
            timestamp = int(fields[0])

            # new log started?
            if not last_timestamp or (timestamp < last_timestamp and timestamp < 1000):
                start_date = None
                if last_timestamp:
                    self._log_found(last_start, linenum-1, last_timestamp, start_date)
                last_start = linenum
            last_timestamp = timestamp


            if not start_date and len(fields) >= 9 and fields[1] == "in" and fields[2] == "4" \
               and 'GNRMC' in fields[4]:
                date = fields[13]
                if date:
                    day = int(date[0:2])
                    month = int(date[2:4])
                    year = 2000 + int(date[4:6])
                    fixtime = fields[5]
                    hour = int(fixtime[0:2])
                    minute = int(fixtime[2:4])
                    second = int(fixtime[4:6])
                    start_date = datetime.datetime(
                        year=year, month=month, day=day,
                        hour=hour, minute=minute, second=second,
                        tzinfo=datetime.timezone.utc)

        self._log_found(last_start, len(self.lines)-1, last_timestamp, start_date)

    def extract(self, lognum, channeltype, channelnum):
        if lognum < 0 or lognum >= len(self.index):
            print(f"log number {lognum} out of range")

        (startline, endline, duration, utc_start) = self.index[lognum]
        for linenum in range(startline, endline+1):
            line = self.lines[linenum]
            fields = line.split(",")
            if len(fields) < 4:
                continue
            if not fields[1] == channeltype:
                continue
            this_line_channel = int(fields[2])
            if this_line_channel != channelnum:
                continue

            if channeltype == "in" or channeltype == "u":
                sys.stdout.write(",".join(fields[4:]))
            else:
                sys.stdout.write(f"{fields[0]},{fields[3]}")

def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} filename [lognum channeltype channelnum]")
        sys.exit(1)

    log = Log(sys.argv[1])

    if len(sys.argv) == 5:
        log.extract(int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))

main()


