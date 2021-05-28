#!/usr/bin/env python3

import pandas as pd
import sys

df = pd.read_csv(sys.argv[1])
df['time_sec'] = df.iloc[:, 0] / 1000
df['current_ma'] = df.iloc[:, 1] / 1000
print(df)
plot = df.plot(x='time_sec', y='current_ma', grid=True, figsize=(20, 10))
plot.set_title('Sony GPS current use')
plot.set_xlabel('Time (sec)')
plot.set_ylabel('Current (mA)')
plot.figure.savefig(sys.argv[1]+".plot.png")
