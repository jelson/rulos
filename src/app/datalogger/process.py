#!/usr/bin/env python3

import sys
import datetime
import pandas as pd
import binascii
import geopandas
from shapely.geometry import Point
import contextily as cx

REF_CHAN = 4
SONY_CHAN = 1
UBLOX_CHAN = 3

class Log:
    def _log_found(self, metadata):
        i = len(self.index)
        duration_min = metadata['duration_msec'] / 1000.0 / 60.0
        if metadata['utc_start']:
            when = "@" + metadata['utc_start'].astimezone().isoformat()
        else:
            when = "// no fix"
        sys.stderr.write(f"Found log {i:2}: lines {metadata['startline']:8} -{metadata['endline']:8}, {duration_min:6.1f}min {when}\n")
        self.index.append(metadata)

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

            # If we detected a new log just started, add the log we were in the
            # middle of parsing to the list of logs
            if not last_timestamp or (timestamp < last_timestamp and timestamp < 1000):
                if last_timestamp:
                    self._log_found({
                        'startline': last_start,
                        'endline': linenum-1,
                        'duration_msec': last_timestamp,
                        'utc_start': start_date,
                    })
                last_start = linenum
                start_date = None
            else:
                if timestamp < last_timestamp:
                    sys.stderr.write(f"WARNING: timestamp {timestamp} came after {last_timestamp}\n")
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

        self._log_found({
            'startline': last_start,
            'endline': len(self.lines)-1,
            'duration_msec': last_timestamp,
            'utc_start': start_date,
        })

    def get_log_metadata(self, lognum):
        if lognum < 0 or lognum >= len(self.index):
            print(f"log number {lognum} out of range")
            sys.exit(1)

        return self.index[lognum]

    def extract(self, lognum, channeltype, channelnum):
        md = self.get_log_metadata(lognum)
        for linenum in range(md['startline'], md['endline']+1):
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

    def checked_nmea(self, split_line):
        orig_line = ','.join(split_line)
        orig_line = orig_line.strip()
        cksplit = orig_line.split('*')
        if len(cksplit) != 2:
            return None

        # make sure the line starts with $
        if len(cksplit[0]) < 1 or cksplit[0][0] != '$':
            return None

        # calculate checksum of everything from $ to *, exclusive
        nmea = cksplit[0][1:]
        calc_cksum = 0
        for s in nmea:
            calc_cksum ^= ord(s)

        # get checksum at end of line
        try:
            sent_cksum = int(cksplit[1], 16)
        except:
            return None

        if calc_cksum == sent_cksum:
            return split_line
        else:
            return None

    def ddmm_to_degrees(self, s, hemisphere):
        whole_degrees = int(float(s) / 100)
        dec_degrees = (float(s) - (whole_degrees * 100)) / 60
        degrees = whole_degrees + dec_degrees
        if hemisphere == "S" or hemisphere == "W":
            return -degrees
        else:
            return degrees

    def parse(self, lognum):
        md = self.get_log_metadata(lognum)

        data = {
            'current_ublox': {},
            'current_sony': {},
            'loc_ublox': {},
            'loc_sony': {},
        }

        for linenum in range(md['startline'], md['endline']+1):
            line = self.lines[linenum]
            fields = line.split(",")

            if len(fields) < 4:
                continue

            timestamp = int(int(fields[0]) / 1000)
            chan = int(fields[2])
            if fields[1] == "curr":
                curr = int(fields[3])
                if chan == SONY_CHAN:
                    data['current_sony'][timestamp] = curr
                elif chan == UBLOX_CHAN:
                    data['current_ublox'][timestamp] = curr
                else:
                    print(f'unknown current channel {chan}')
                    sys.exit(1)

            elif fields[1] == "in":
                nmea = self.checked_nmea(fields[4:])
                if not nmea:
                    continue
                if nmea[0].endswith("GGA"):
                    # valid fix?
                    if nmea[6] == "0":
                        loc = None
                    else:
                        lat = self.ddmm_to_degrees(nmea[2], nmea[3])
                        lon = self.ddmm_to_degrees(nmea[4], nmea[5])
                        loc = Point(lon, lat)

                    if chan == SONY_CHAN:
                        data['loc_sony'][timestamp] = loc
                    elif chan == UBLOX_CHAN:
                        data['loc_ublox'][timestamp] = loc

        gdf = geopandas.GeoDataFrame(pd.DataFrame(data))
        for geocol in ['loc_sony', 'loc_ublox']:
            gdf[geocol] = geopandas.GeoSeries(gdf[geocol])
            gdf = gdf.set_geometry(geocol)
            gdf = gdf.set_crs(epsg=4326)
        return gdf

    def map(self, lognum):
        df = self.parse(lognum)
        gdf = df.set_geometry('loc_ublox')
        df_wm = gdf.to_crs(epsg=3857)
        ax = df_wm.plot(color='red', markersize=1, figsize=(50, 50))
        cx.add_basemap(ax, zoom=15)
        ax.figure.tight_layout()
        ax.figure.savefig("test.png")


def usage():
    print("usage:")
    print(f"{sys.argv[0]} <filename>")
    print(f"{sys.argv[0]} <filename> extract lognum channeltype channelnum")
    print(f"{sys.argv[0]} <filename> map lognum")
    sys.exit(1)

def main():
    if len(sys.argv) < 2:
        usage()

    log = Log(sys.argv[1])

    # no args but filename, just print summary of trips in log file
    if len(sys.argv) == 2:
        return

    # log extraction mode
    if len(sys.argv) == 6 and sys.argv[2] == "extract":
        log.extract(int(sys.argv[3]), sys.argv[4], int(sys.argv[5]))
        return

    if len(sys.argv) == 4 and sys.argv[2] == "map":
        log.map(int(sys.argv[3]))
        return

    usage()


main()
