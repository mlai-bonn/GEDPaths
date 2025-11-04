import os
import torch
from torch_geometric.data import InMemoryDataset
from plot_graph import plot_edit_path, find_processed_pt
from python_src.converter.GEDPathsInMemory import GEDPathsInMemoryDataset
import argparse


def main():
    parser = argparse.ArgumentParser(description='Plot a single edit path using the same args as plot_edit_path_stats')
    default_dir = 'Results/Paths/F2/MUTAG'
    parser.add_argument('directory', nargs='?', default=default_dir,
                        help='Root directory for a specific Results/Paths_{strategy}/{method}/{database} (default: %(default)s)')
    parser.add_argument('-s', '--path_strategy', dest='strategy', default='Rnd_d-IsoN',
                        help="Generating path strategy name used inside Results/Paths_{strategy}/. Default: 'Rnd_d-IsoN'.")
    parser.add_argument('-d', '--db', dest='database', default='MUTAG',
                        help="Database name used inside Results/Paths_{strategy}/{method}/{database}/. Default: 'MUTAG'.")
    parser.add_argument('-m', '--method', dest='method', default='F2',
                        help="Method name used inside Results/Paths_{strategy}/{method}/{database}/. Default: 'F2'.")
    parser.add_argument('--save', dest='save', action='store_true', help='Save generated plot (default)')
    parser.add_argument('--no-save', dest='save', action='store_false', help='Do not save generated plot')
    parser.set_defaults(save=True)
    parser.add_argument('--show', action='store_true', help='Display plot interactively')
    parser.add_argument('--start', type=int, default=3, help='Start index for paths (default: 3)')
    parser.add_argument('--end', type=int, default=77, help='End index for paths (default: 77)')
    args = parser.parse_args()

    # Determine directory: if positional default was used, build from strategy/method/database
    if args.directory == default_dir:
        directory = os.path.join('Results', f'Paths_{args.strategy}', args.method, args.database)
    else:
        directory = args.directory
        if args.strategy and 'Paths' in directory and 'Paths_' not in directory:
            directory = directory.replace('Paths', f'Paths_{args.strategy}')

    # Setup paths used by the original script
    root_dir = directory
    processed_dir = os.path.join(root_dir, 'processed')
    edit_path_file = os.path.join(root_dir, f"{args.database}_edit_paths_data_current.txt")
    output_path = os.path.join(root_dir, 'Plots')

    os.makedirs(output_path, exist_ok=True)

    # Load processed dataset
    processed_pt = find_processed_pt(processed_dir)
    assert processed_pt is not None, f"Processed .pt file not found in {processed_dir}"
    ds = GEDPathsInMemoryDataset(root_dir, path=processed_pt, edit_path_data=edit_path_file)

    start = args.start
    end = args.end
    path_graphs = ds.get_path_graphs(start, end)
    tmp_edit_operations = ds.get_path_operations(start, end)
    edit_operations = []
    for op in tmp_edit_operations:
        edit_operations.append(op.operation["raw"])  # extract raw dict

    out_file = os.path.join(output_path, f"edit_path_{start}_{end}.png")
    plot_edit_path(path_graphs, edit_operations, output=out_file)
    print(f"Edit path plot saved to {out_file}")


if __name__ == '__main__':
    main()
