#!/usr/bin/env python3

import pandas as pd
import sys

dfs = {}
for f in sys.argv[1:]:
    df = pd.read_csv(f)
    dfs[f] = df
    df['time_sec'] = df.iloc[:, 0] / 1000
    df['current_ma'] = df.iloc[:, 1] / 1000
    df['current_ma'] = df['current_ma'].apply(lambda x: x if x < 350 else 0)
    df['curr_ma_90secroll'] = df['current_ma'].rolling(90, center=True).mean()
    volts = None
    if 'sony' in f:
        bonus = 6
        volts = 0.8
    if 'ublox' in f:
        volts = 3.3
        bonus = 0
    df['power_mw'] = df['current_ma'] * volts + bonus
    df['power_mw_90secroll'] = df['power_mw'].rolling(90).mean()
    

print(df)

title = ""
ax = None
for f, df in dfs.items():
    ax = df.plot(x='time_sec', y=['power_mw', 'power_mw_90secroll'],
                 grid=True, figsize=(20, 10), ax=ax)
    title += f"{f}: Mean Power {df['power_mw'].mean():.2f}mw\n"

ax.set_title(title)
ax.set_xlabel('Time (sec)')
ax.set_ylabel('Current (mA)')
ax.figure.savefig(sys.argv[1]+".plot.png")
