#!/usr/bin/env python3

import os
import sys
import datetime
import pandas as pd
import numpy as np
import binascii
import geopandas
from shapely.geometry import Point
import contextily as cx
import matplotlib.pyplot as plt

cx.set_cache_dir("/tmp/cached-tiles")

SONY_UBLOX_CONF = {
    'reference': {
        'channel': 4,
    },
    'Sony': {
        'channel': 1,
        'volts': 0.8,
        'extra_power_mw': 6, # to account for the LNA, TCXO not measured
    },
    'uBlox': {
        'channel': 3,
        'volts': 3.3,
    },
}

QUECTEL_CONF = {
    'Quectel': {
        'channel': 3,
        'volts': 3.3,
    },
}

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


            if not start_date and len(fields) >= 9 and fields[1] == "in" and 'GNRMC' in fields[4] \
               and fields[6] == 'A':
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

    def parse(self, lognum, conf):
        datasets = {}

        md = self.get_log_metadata(lognum)
        nmea_good = 0
        nmea_bad = 0

        for linenum in range(md['startline'], md['endline']+1):
            line = self.lines[linenum]
            fields = line.split(",")

            if len(fields) < 4:
                continue

            # get channel number and find corresponding dataset
            chan = int(fields[2])

            if not chan in datasets:
                datasets[chan] =  {
                'current_ua': {},
                'loc': {},
                'speed_mph': {},
                'num_sats': {},
            }

            dataset = datasets[chan]

            # get time in seconds, rounded
            timestamp = int(int(fields[0]) / 1000)

            if fields[1] == "curr":
                curr = int(fields[3])
                dataset['current_ua'][timestamp] = curr

            elif fields[1] == "in":
                nmea = self.checked_nmea(fields[4:])
                if not nmea:
                    nmea_bad += 1
                    continue
                nmea_good += 1
                if nmea[0].startswith("$G") and nmea[0].endswith("RMC"):
                    if nmea[2] == "A" or nmea[2] == "D":
                        speed_mph = float(nmea[7]) * 1.151
                    else:
                        speed_mph = None
                    dataset['speed_mph'][timestamp] = speed_mph
                if nmea[0].endswith("GGA"):
                    # valid fix?
                    if nmea[6] == "0":
                        loc = None
                    else:
                        lat = self.ddmm_to_degrees(nmea[2], nmea[3])
                        lon = self.ddmm_to_degrees(nmea[4], nmea[5])
                        loc = Point(lon, lat)

                    dataset['loc'][timestamp] = loc
                    dataset['num_sats'][timestamp] = int(nmea[7])

        retval = {}
        for dutname, dutconf in conf.items():
            # convert dictionaries to pandas dataframes
            chan = dutconf['channel']
            gdf = geopandas.GeoDataFrame(pd.DataFrame(datasets[chan])).sort_index()

            # set up location field as a geopandas geodataframe
            gdf['loc'] = geopandas.GeoSeries(gdf['loc'])
            gdf = gdf.set_geometry('loc')
            gdf = gdf.set_crs(epsg=4326)

            # remove insane current measurements
            gdf['current_ua'] = gdf['current_ua'].apply(lambda x: x if x < 40000 else np.nan)

            # convert current to power
            if 'volts' in dutconf:
                gdf['power_uw'] = (gdf['current_ua'] * dutconf['volts']) + (1000 * dutconf.get('extra_power_mw', 0))

            retval[dutname] = gdf

        sys.stderr.write(f"parsing complete -- {nmea_bad}/{nmea_good+nmea_bad} NMEA sentence errors\n")
        return retval

    def map(self, lognum, conf):
        datasets = self.parse(lognum, conf)

        for dutname, dutconf in conf.items():
            df = datasets[dutname]
            df = df.to_crs(epsg=3857)
            ax = df.plot(color='red', markersize=1, figsize=(40, 40))

            for secs in range(100, df.index.max(), 100):
                loc = df.loc[secs, 'loc']
                if loc:
                    ax.annotate(xy=[loc.x, loc.y],
                                text=f"{secs}",
                                xytext=[10, 0],
                                textcoords='offset points',
                                arrowprops=dict(arrowstyle='-'),
                    )

            cx.add_basemap(ax,
                           zoom=15,
                           crs=df.crs,
                           source=cx.providers.OpenStreetMap.Mapnik)
            ax.set_title(f"Drive map - {dutname} - {sys.argv[1]}")
            ax.figure.tight_layout()
            ax.axis('off')
            ax.figure.savefig(f"{sys.argv[1]}.map-{dutname}.png", dpi=200, bbox_inches='tight')

    def currents(self, lognum, conf):
        datasets = self.parse(lognum, conf)

        # create current plot on top, speed plot next, then num sats
        fig, ((currplot, satplot, speedplot)) = plt.subplots(
            3, 1,
            figsize=(30, 15),
            sharex=True,
        )

        for dutname, dutconf in conf.items():
            df = datasets[dutname]
            df['power_mw'] = df['power_uw'] / 1000.0
            df['power_mw_90secroll'] = df['power_mw'].rolling(90, center=True).mean()

            # plot power green where we have a gps lock, red where we do not
            currs = df.loc[~pd.isna(df['power_mw'])]
            unlocked = currs.copy()
            unlocked.loc[~pd.isna(unlocked['loc']), 'power_mw'] = np.nan
            locked = currs.copy()
            locked.loc[pd.isna(locked['loc']), 'power_mw'] = np.nan

            unlocked['power_mw'].plot(grid=True, ax=currplot, color='red')
            locked['power_mw'].plot(grid=True, ax=currplot, color='green')
            df['power_mw_90secroll'].plot(grid=True, ax=currplot, color='blue')

            # add annotation with average power
            ann = f"{dutname} average power: {df['power_mw'].mean():.2f}"
            currplot.annotate(xy=[0, df['power_mw'].max()], text=ann, size=15)

        for df in datasets.values():
            df['speed_mph'].plot(grid=True, ax=speedplot)
        speedplot.grid(which='major', linestyle='-', color='black')
        speedplot.grid(which='minor', linestyle=':', color='black')
        speedplot.set_ylabel('Speed (mph)')

        for df in datasets.values():
            df['num_sats'].plot(grid=True, ax=satplot)
        satplot.grid(which='major', linestyle='-', color='black')
        satplot.grid(which='minor', linestyle=':', color='black')
        satplot.set_ylabel('Num Sats')

        currplot.minorticks_on()
        currplot.grid(which='major', linestyle='-', color='black')
        currplot.grid(which='minor', linestyle=':', color='black')
        currplot.set_title(f'Current Use - {os.path.basename(sys.argv[1])}')
        currplot.set_ylabel('Power (mW)')
        currplot.figure.tight_layout()

        speedplot.set_xlabel('Time (sec)')
        fig.savefig(sys.argv[1]+".currents-plot.png")


def usage():
    print("usage:")
    print(f"{sys.argv[0]} <filename>")
    print(f"{sys.argv[0]} <filename> extract <lognum> <channeltype> <channelnum>")
    print(f"{sys.argv[0]} <filename> map <lognum>")
    print(f"{sys.argv[0]} <filename> currents <lognum>")
    sys.exit(1)

def main():
    if len(sys.argv) < 2:
        usage()

    #conf = SONY_UBLOX_CONF
    conf = QUECTEL_CONF

    log = Log(sys.argv[1])

    # no args but filename, just print summary of trips in log file
    if len(sys.argv) == 2:
        return

    # log extraction mode
    if len(sys.argv) == 6 and sys.argv[2] == "extract":
        log.extract(int(sys.argv[3]), sys.argv[4], int(sys.argv[5]))
        return

    if len(sys.argv) == 4 and sys.argv[2] == "map":
        log.map(int(sys.argv[3]), conf)
        return

    if len(sys.argv) == 4 and sys.argv[2] == "currents":
        log.currents(int(sys.argv[3]), conf)
        return

    usage()


main()
