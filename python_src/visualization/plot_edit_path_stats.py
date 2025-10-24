#!/usr/bin/env python3
"""
Visualization for edit path statistics CSVs and per-path positions CSVs.

Usage:
  python3 plot_edit_path_stats.py /path/to/Results/Paths/F2/MUTAG/Evaluation --save --show

This script will:
 - read all .csv files in the provided directory
 - for ordinary statistic CSVs (single-column with header 'value') it creates a line plot and a histogram (existing behavior)
 - for per-path positions CSVs (files that end with '_Positions.csv' or contain 'Positions' in the filename), it will:
    - parse each row's comma-separated integer positions
    - create a counts-per-position summary CSV: <basename>_counts.csv with columns 'position,count'
    - plot counts per position (line) and save as <basename>_counts.png
    - create a heatmap (paths x positions) showing presence (1) or absence (0) and save as <basename>_heatmap.png

Dependencies: pandas, matplotlib, seaborn (seaborn optional but improves heatmap)
Install: pip3 install pandas matplotlib seaborn
"""

import argparse
import glob
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import importlib
import math

# module-level path for Python outputs; set in main()
python_output_dir = None

def py_out(filename: str) -> str:
    """Return the absolute path under the python output directory for the given filename.
    Ensures the directory exists.
    """
    global python_output_dir
    if python_output_dir is None:
        raise RuntimeError('python_output_dir not set; call from main after initialization')
    os.makedirs(python_output_dir, exist_ok=True)
    return os.path.join(python_output_dir, filename)


try:
    sns = importlib.import_module('seaborn')
    _HAS_SEABORN = True
except Exception:
    sns = None
    _HAS_SEABORN = False


def plot_csv_file(csv_path: str, save: bool = True, show: bool = False):
    basename = os.path.splitext(os.path.basename(csv_path))[0]
    dirpath = os.path.dirname(csv_path)

    # Positions files detection
    if 'Positions' in basename:
        counts_df = _process_positions_file(csv_path, dirpath, basename, save=save, show=show)
        # only return when counts_df was successfully produced
        if counts_df is not None:
            return (basename, counts_df)
        return None

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Failed to read {csv_path}: {e}")
        return

    if 'value' not in df.columns:
        print(f"CSV {csv_path} does not contain a 'value' column. Columns: {list(df.columns)}")
        return

    values = df['value'].dropna().astype(float)
    if values.empty:
        print(f"CSV {csv_path} has no numeric values to plot.")
        return

    plt.figure(figsize=(12, 5))

    # Line plot
    plt.subplot(1, 2, 1)
    plt.plot(values.values, marker='o', linestyle='-')
    plt.title(f"{basename} - line")
    plt.xlabel('index')
    plt.ylabel('value')

    # Histogram
    plt.subplot(1, 2, 2)
    plt.hist(values.values, bins=min(50, max(5, len(values)//2)))
    plt.title(f"{basename} - histogram")
    plt.xlabel('value')
    plt.ylabel('count')

    plt.tight_layout()

    outpath = os.path.join(dirpath, basename + '.png')
    if save:
        try:
            outpath = py_out(basename + '.png')
            plt.savefig(outpath)
            print(f"Saved plot to {outpath}")
        except Exception as e:
            print(f"Failed to save plot for {csv_path}: {e}")

    if show:
        plt.show()
    else:
        plt.close()


def _parse_positions_column(series: pd.Series):
    """Parse a pandas Series of strings (or NaN) where each entry is comma-separated integers.
    Returns a list of lists of ints.
    """
    positions = []
    for val in series.fillna(''):
        if isinstance(val, (int, float)):
            # single numeric value (unlikely), treat as single position if integer
            try:
                iv = int(val)
                positions.append([iv])
            except Exception:
                positions.append([])
            continue
        s = str(val).strip()
        if s == '':
            positions.append([])
            continue
        parts = [p.strip() for p in s.split(',') if p.strip() != '']
        row = []
        for p in parts:
            try:
                row.append(int(p))
            except Exception:
                # ignore non-integer token but warn
                print(f"Warning: ignoring non-integer token '{p}' in positions CSV")
        positions.append(row)
    return positions


def _process_positions_file(csv_path: str, dirpath: str, basename: str, save: bool = True, show: bool = False):
    # Read positions CSV robustly: prefer reading raw file lines (each line is one path's positions).
    try:
        with open(csv_path, 'r') as f:
            lines = [l.rstrip('\n') for l in f]
    except Exception as e:
        print(f"Failed to open positions CSV {csv_path}: {e}")
        # fallback: try pandas default reader
        try:
            df = pd.read_csv(csv_path)
        except Exception as e2:
            print(f"Failed to read positions CSV {csv_path} with pandas fallback: {e2}")
            return None
        colname = df.columns[0]
        series = df[colname].astype(str)
    else:
        # remove possible header line 'positions'
        if lines and lines[0].strip().lower() == 'positions':
            lines = lines[1:]
        series = pd.Series([l for l in lines])

    positions = _parse_positions_column(series)
    if not positions:
        print(f"No position data found in {csv_path}")
        return None

    # determine max position to size arrays
    max_pos = -1
    for row in positions:
        if row:
            max_row = max(row)
            if max_row > max_pos:
                max_pos = max_row
    if max_pos < 0:
        print(f"Positions CSV {csv_path} has no numeric positions")
        return None

    n_paths = len(positions)
    width = max_pos + 1
    # build presence matrix
    mat = np.zeros((n_paths, width), dtype=int)
    for i, row in enumerate(positions):
        for p in row:
            if 0 <= p < width:
                mat[i, p] = 1

    # counts per position
    counts = np.sum(mat, axis=0)
    counts_df = pd.DataFrame({'position': np.arange(width), 'count': counts})
    counts_csv = py_out(basename + '_counts.csv')
    counts_df.to_csv(counts_csv, index=False)
    print(f"Wrote position counts CSV: {counts_csv}")

    # plot counts line
    plt.figure(figsize=(10,4))
    plt.plot(counts_df['position'], counts_df['count'], marker='o')
    plt.title(f"{basename} - counts per position")
    plt.xlabel('position')
    plt.ylabel('count')
    plt.grid(True)
    out_counts_png = py_out(basename + '_counts.png')
    if save:
        plt.savefig(out_counts_png)
        print(f"Saved counts plot: {out_counts_png}")
    if show:
        plt.show()
    else:
        plt.close()

    # heatmap (may be large) - compute integer figure size to avoid float warnings
    heatmap_w = min(20, max(6, width // 2))
    heatmap_h = min(20, max(4, max(1, n_paths // 10)))
    plt.figure(figsize=(int(heatmap_w), int(heatmap_h)))
    if _HAS_SEABORN:
        sns.heatmap(mat, cbar=True)
    else:
        plt.imshow(mat, aspect='auto', interpolation='nearest', cmap='Greys')
        plt.colorbar()
    plt.title(f"{basename} - heatmap (paths x positions)")
    plt.xlabel('position')
    plt.ylabel('path index')
    out_heatmap = py_out(basename + '_heatmap.png')
    if save:
        plt.savefig(out_heatmap)
        print(f"Saved heatmap: {out_heatmap}")
    if show:
        plt.show()
    else:
        plt.close()

    # return the counts DataFrame for possible aggregation
    return counts_df


def bucket_combined_counts_df(combined_df: pd.DataFrame, n_buckets: int = 10) -> pd.DataFrame:
    """Given a combined per-position DataFrame with a 'position' column and operation columns,
    aggregate counts into n_buckets across the full position range.
    Returns a DataFrame with columns: 'bucket' (0..n_buckets-1) and one column per operation with summed counts.
    """
    # determine global max position
    max_pos = int(combined_df['position'].max())
    total_positions = max_pos + 1
    bucket_size = math.ceil(total_positions / n_buckets)

    buckets = list(range(n_buckets))
    bucket_df = pd.DataFrame({'bucket': buckets})
    # for each operation column, sum counts falling into each bucket
    op_cols = [c for c in combined_df.columns if c != 'position']
    for op in op_cols:
        sums = []
        for b in buckets:
            start = b * bucket_size
            end = min((b + 1) * bucket_size - 1, max_pos)
            mask = (combined_df['position'] >= start) & (combined_df['position'] <= end)
            s = int(combined_df.loc[mask, op].sum())
            sums.append(s)
        bucket_df[op] = sums
    return bucket_df


def plot_buckets_stacked(bucket_df: pd.DataFrame, out_png: str, normalize: bool = False, save: bool = True, show: bool = False):
    """Plot a stacked bar chart from bucket_df (bucket column + operation columns).
    If normalize=True, convert counts to proportions per bucket.
    """
    df = bucket_df.copy()
    op_cols = [c for c in df.columns if c != 'bucket']
    if normalize:
        # compute row-wise sum and divide
        row_sum = df[op_cols].sum(axis=1).replace(0, 1)
        for c in op_cols:
            df[c] = df[c] / row_sum

    # stacked bar
    fig, ax = plt.subplots(figsize=(12, 5))
    bottom = np.zeros(len(df))
    x = df['bucket'].values
    for c in op_cols:
        ax.bar(x, df[c].values, bottom=bottom, label=c)
        bottom = bottom + df[c].values
    ax.set_xlabel('bucket (path segment)')
    ax.set_ylabel('proportion' if normalize else 'count')
    ax.set_title(('Normalized ' if normalize else '') + 'Operation distribution across path buckets')
    ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.tight_layout()
    if save:
        plt.savefig(out_png)
        print(f"Saved buckets plot: {out_png}")
    if show:
        plt.show()
    else:
        plt.close()


def main():
    parser = argparse.ArgumentParser(description='Plot edit path statistics CSVs')
    parser.add_argument('directory', nargs='?', default='Results/Paths/F2/MUTAG/Evaluation',
                        help='Directory containing CSV files (Evaluation folder). Defaults to %(default)s')
    parser.add_argument('--save', dest='save', action='store_true', help='Save plots as PNGs (default)')
    parser.add_argument('--no-save', dest='save', action='store_false', help='Do not save plots')
    parser.set_defaults(save=True)
    parser.add_argument('--show', action='store_true', help='Display plots interactively')
    parser.add_argument('--buckets', type=int, default=10, help='Number of buckets to aggregate positions into for bucketed plots (default: 10)')
    args = parser.parse_args()

    directory = args.directory
    if not os.path.isdir(directory):
        print(f"Directory not found: {directory}")
        sys.exit(1)

    # set python output directory as a sibling to the evaluation folder (same level as Evaluation)
    global python_output_dir
    parent_dir = os.path.dirname(os.path.abspath(directory))
    python_output_dir = os.path.join(parent_dir, 'Evaluation_Python')
    os.makedirs(python_output_dir, exist_ok=True)

    csv_files = sorted(glob.glob(os.path.join(directory, '*.csv')))
    if not csv_files:
        print(f"No CSV files found in {directory}")
        sys.exit(0)

    # collect per-positions counts across files so we can make a combined plot
    positions_counts = {}
    for csv in csv_files:
        try:
            ret = plot_csv_file(csv, save=args.save, show=args.show)
        except Exception as e:
            print(f"Error processing {csv}: {e}")
            ret = None
        if ret and isinstance(ret, tuple) and len(ret) == 2:
            basename, counts_df = ret
            # normalize column name (strip trailing '_Positions')
            key = basename.replace('_Positions', '')
            positions_counts[key] = counts_df

    # If we have multiple operation counts, create a combined overlay plot and CSV
    if positions_counts:
        # determine global max position
        max_pos = 0
        for df in positions_counts.values():
            if not df.empty:
                max_p = int(df['position'].max())
                if max_p > max_pos:
                    max_pos = max_p
        positions = np.arange(max_pos + 1)
        combined = pd.DataFrame({'position': positions})
        for name, df in positions_counts.items():
            # merge counts; fill missing positions with 0
            merged = pd.merge(combined[['position']], df, on='position', how='left')
            combined[name] = merged['count'].fillna(0).astype(int)

        combined_csv = py_out('Combined_Operations_counts.csv')
        combined.to_csv(combined_csv, index=False)
        print(f"Wrote combined counts CSV: {combined_csv}")

        # plot overlay
        plt.figure(figsize=(12, 5))
        for name in [k for k in combined.columns if k != 'position']:
            plt.plot(combined['position'], combined[name], marker='o', label=name)
        plt.title('Combined operation counts per position')
        plt.xlabel('position')
        plt.ylabel('count')
        plt.grid(True)
        plt.legend()
        out_combined = py_out('Combined_Operations_counts.png')
        if args.save:
            plt.savefig(out_combined)
            print(f"Saved combined plot: {out_combined}")
        if args.show:
            plt.show()
        else:
            plt.close()

        # Bucketed counts - absolute and normalized stacked plots
        bucketed_df = bucket_combined_counts_df(combined, n_buckets=args.buckets)
        bucketed_csv = py_out('Bucketed_Operations_counts.csv')
        bucketed_df.to_csv(bucketed_csv, index=False)
        print(f"Wrote bucketed counts CSV: {bucketed_csv}")

        # absolute stacked
        plot_buckets_stacked(bucketed_df, py_out('Bucketed_Operations_counts_absolute.png'), normalize=False, save=args.save, show=args.show)
        # normalized stacked
        plot_buckets_stacked(bucketed_df, py_out('Bucketed_Operations_counts_normalized.png'), normalize=True, save=args.save, show=args.show)


if __name__ == '__main__':
    main()
