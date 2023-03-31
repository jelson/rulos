#!/usr/bin/env python3

import matplotlib
import numpy as np
import os
import pandas as pd
import sys

matplotlib.use('Agg')

pd.set_option("display.precision", 9)

def fit_and_correct(d):
    # compute the phase difference between the timestamper's clock and the PPS
    fit = np.polyfit(d.index, d['timestamp'], deg=1)
    print(f"fit: {fit}")

    # apply a phase error correction and phase/freq correction
    d['phase_corrected_timestamp'] = d['timestamp'] - fit[1]
    d['phase_freq_corrected_timestamp'] = d['timestamp'] - (fit[0]*d.index + fit[1])

    return (d, fit)

def reject_outliers(d):
    has_outliers = True
    total_points_dropped = 0
    drop_iterations = 1

    # iteratively compute a mean of the points we have, drop outliers that are
    # more than 4 std dev away from the mean, and recompute the mean until it
    # converges and there are no more outliers rejected.
    while has_outliers:
        # get a fit of the remaining data
        (d, fit) = fit_and_correct(d)

        summary = d['phase_freq_corrected_timestamp'].describe()
        print(f"iteration {drop_iterations} summary of post-correction errors:\n{summary}")

        # drop outliers 4 stddev's away from the mean in either direction
        points_before = d.shape[0]
        d['abs_error'] = (d['phase_freq_corrected_timestamp'] - summary['mean']).abs()
        d = d.loc[d['abs_error'] < 4 * summary['std']].copy()
        num_dropped = points_before - d.shape[0]
        if num_dropped > 0:
            total_points_dropped += num_dropped
            drop_iterations += 1
        else:
            has_outliers = False

    print(f"Dropped {total_points_dropped} outliers over {drop_iterations} iterations")
    return d
    
def analyze(d, title):
    d = reject_outliers(d)
    (d, fit) = fit_and_correct(d)

    slope_error = fit[0] - 1.0
    error_ratio = 1/slope_error
    ppb = slope_error*1e9
    error_str = f"Error: {slope_error:.2e} ({ppb:.2f}ppb)"
    print(f"{title}: {error_str}")

    # create nanosecond-based column for plotting
    ROLL_SEC = 30
    d['error_ns'] = 1e9 * (d['phase_corrected_timestamp'] - d.index)
    roll_err_col = f'error_ns_{ROLL_SEC}_avg'
    d[roll_err_col] = d['error_ns'].rolling(ROLL_SEC, center=True).mean()
    d['fit'] = 1e9 * (d.index * slope_error)
    ax = d.plot(y='error_ns', style='.', ms=5, zorder=2, figsize=(20, 10))
    d.plot(y=roll_err_col, ms=1, ax=ax, zorder=1)
    d.plot(y='fit', ms=1, ax=ax, zorder=1, grid=True)
    ax.set_title(f'{title}\n{error_str}')
    ax.set_xlabel("Experiment Time (sec)")
    ax.set_ylabel("Cumulative Phase Error (ns)")
    ax.minorticks_on()
    ax.grid(which='minor', linestyle=':')
    ax.grid(which='major', color='red')
    ax.figure.savefig(f'{title}.time.plot.png', dpi=150, bbox_inches='tight')

    d['wander_error_ns'] = 1e9 * d['phase_freq_corrected_timestamp']
    ax = d.plot(y=['wander_error_ns'], grid=True, figsize=(20, 10))
    ax.set_title(f'{title}\nResidual Error After Post-Hoc Calibration\n{error_str}')
    ax.set_xlabel("Experiment Time (sec)")
    ax.set_ylabel("Residual Error (ns)")
    ax.figure.savefig(f'{title}.residuals.plot.png')

def append(pulses, pulse_number, timestamp):
    pulses.append({'pulse_number': pulse_number, 'timestamp': timestamp})
    print(f"pulse {pulse_number} at {timestamp}")
    assert(timestamp - pulse_number < 1 and timestamp - pulse_number > 0)

def parse(filename):
    title = os.path.basename(filename)

    lines = open(filename, "r", encoding="ascii").readlines()

    first_time = None
    times = {}
    for line in lines:
        line = line.rstrip()

        # skip comments
        if '#' in line:
            continue

        (channel, curr_time) = line.split()
        curr_time = float(curr_time)
        if not first_time:
            first_time = curr_time
        curr_time -= int(first_time)
        times[int(curr_time)] = curr_time
        
    # convert to dataframe
    d = pd.DataFrame(times.items())
    d = d.set_index(0)
    d.columns = ['timestamp']
    analyze(d, title)

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <filename> [filename...]")
        sys.exit(1)

    for filename in sys.argv[1:]:
        parse(filename)

if __name__ == '__main__':
    main()
