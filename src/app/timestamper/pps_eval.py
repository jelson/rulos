#!/usr/bin/env python3

#
# Script for analyzing timestamper data when the input is a PPS signal
#

import argparse
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import sys

matplotlib.use('Agg')

pd.set_option("display.precision", 9)

def fit_and_correct(d, chan, debug=False):
    # compute the phase difference between the timestamper's clock and the PPS
    fitdata = d[chan].dropna()
    fit = np.polyfit(fitdata.index, fitdata.values, deg=1)

    if debug:
        print(f"Channel {chan} fit: {fitdata.size} points (no data for {d.index.size-fitdata.size}), freq error={1.0-fit[0]:.3e}, b={fit[1]:.9}")

    # apply a phase error correction and phase/freq correction
    d[f'{chan}_phase_corrected_timestamp'] = d[chan] - fit[1]
    d[f'{chan}_phase_freq_corrected_timestamp'] = d[chan] - (fit[0]*d.index + fit[1])

    return (d, fit)

def reject_outliers(d, chan):
    has_outliers = True
    total_points_dropped = 0
    drop_iterations = 0

    print(f"Channel {chan} outlier rejection:")

    # iteratively compute a mean of the points we have, drop outliers that are
    # more than 4 std dev away from the mean, and recompute the mean until it
    # converges and there are no more outliers rejected.
    while has_outliers:
        drop_iterations += 1
        # get a fit of the remaining data
        (d, fit) = fit_and_correct(d, chan)

        summary = d[f'{chan}_phase_freq_corrected_timestamp'].describe()

        # drop outliers 4 stddev's away from the mean error in either direction
        abs_error = (d[f'{chan}_phase_freq_corrected_timestamp'] - summary['mean']).abs()
        to_drop = d.loc[abs_error > 10 * summary['std']]

        if to_drop.size > 0:
            d.loc[to_drop.index, chan] = None
            total_points_dropped += to_drop.size
            print(f"   Iteraton {drop_iterations}: dropped {to_drop.size}")
        else:
            print(f"   Iteraton {drop_iterations}: no drops, done")
            has_outliers = False

    print(f"   Dropped {total_points_dropped} outliers over {drop_iterations} iterations")
    return d

def analyze(args, filename, d):
    channels = d.columns
    base_title = os.path.basename(filename) + "\n"

    # Reject outliers, if requested
    if args.reject_outliers:
        for chan in channels:
            d = reject_outliers(d, chan)

    # Plot the main pps data
    fig, ax = plt.subplots(figsize=(20, 10))
    title = base_title
    for chan in channels:
        (d, fit) = fit_and_correct(d, chan, debug=True)
        slope_error = fit[0] - 1.0
        error_ratio = 1/slope_error
        ppb = slope_error*1e9
        title += f"Channel {chan} frequency error: {slope_error:.2e} ({ppb:.2f}ppb)\n"

        # create nanosecond-based column for plotting and take the rolling average
        err_col = f'Channel {chan} phase error (ns)'
        d[err_col] = 1e9 * (d[f'{chan}_phase_corrected_timestamp'] - d.index)
        roll_err_col = f'Channel {chan} phase error (ns), {args.rolling_average}-sec avg'
        d[roll_err_col] = d[err_col].rolling(args.rolling_average, center=True).mean()
        d.plot(ax=ax, y=roll_err_col, ms=1, zorder=1, alpha=0.5)

        # Plot raw if requested
        if args.plot_raw:
            d.plot(ax=ax, y=err_col, style='.', ms=5, zorder=2)

        # Plot fit if requested
        if args.plot_fit:
            fit_chan = f'Channel {chan} fit'
            d[fit_chan] = 1e9 * (d.index * slope_error)
            d.plot(ax=ax, y=fit_chan, ms=1, zorder=1)

    ax.set_title(title)
    ax.set_xlabel("Experiment Time (sec)")
    ax.set_ylabel("Cumulative Phase Error (ns)")
    ax.minorticks_on()
    ax.grid(which='minor', linestyle=':')
    ax.grid(which='major', color='red')
    ax.figure.savefig(f'{filename}.time.plot.png', dpi=150, bbox_inches='tight')

    # Plot frequency-corrected data
    if args.plot_freq_corrected:
        fig, ax = plt.subplots(figsize=(20, 10))
        title = base_title + "Residual Phase Error after Post-Hoc Calibration\n"
        for chan in channels:
            wander_col = f'Channel {chan} phase error (ns), {args.rolling_average}-sec avg'
            d[wander_col] = 1e9 * d[f'{chan}_phase_freq_corrected_timestamp']
            d[wander_col] = d[wander_col].rolling(args.rolling_average, center=True).mean()
            d.plot(ax=ax, y=wander_col, alpha=0.5)
        ax.set_title(title)
        ax.set_xlabel("Experiment Time (sec)")
        ax.set_ylabel("Residual Error (ns)")
        ax.minorticks_on()
        ax.grid(which='minor', linestyle=':')
        ax.grid(which='major', color='red')
        ax.figure.savefig(f'{filename}.residuals.plot.png', dpi=150, bbox_inches='tight')

def parse(filename):
    d = pd.read_csv(filename, names=['channel', 'timestamp'],
                    skip_blank_lines=True,
                    delimiter=' ', comment='#')
    d = d.dropna()
    d = d.astype({
        'channel': int,
        'timestamp': float,
    })

    # allow only one timestamp per second per channel
    d['pulsenum'] = d['timestamp'].astype(int)
    d = d.groupby(['channel', 'pulsenum']).first().reset_index()

    # pivot so each channel is a separate column
    d = d.pivot(columns='channel', values='timestamp', index='pulsenum')
    return d

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--reject-outliers',
        '-o',
        action='store_true',
        default=False,
        help='Iteratively reject outliers after fitting',
    )
    parser.add_argument(
        '--plot-fit',
        action='store_true',
        default=False,
        help='Plot best fit line to data',
    )
    parser.add_argument(
        '--plot-freq-corrected',
        action='store_true',
        default=False,
        help='Plot the data with post-hoc frequency correction applied',
    )
    parser.add_argument(
        '--plot-raw',
        action='store_true',
        default=False,
        help='Plot raw data in addition to rolling average',
    )
    parser.add_argument(
        '--rolling-average',
        '-r',
        action='store',
        type=int,
        default=30,
        help='Rolling average filter to apply (seconds)',
    )
    parser.add_argument(
        'input_filenames',
        action='store',
        nargs='+',
        help='Timestamper output files to process',
    )
    return parser.parse_args(sys.argv[1:])

def main():
    args = get_args()

    for filename in args.input_filenames:
        d = parse(filename)
        analyze(args, filename, d)

if __name__ == '__main__':
    main()
