#!/usr/bin/env python3
"""
Test loader for .pt files produced by the converter.

Loads `Results/Paths/REFINE/MUTAG/MUTAG_edit_paths.pt` using
`load_pt_to_pyg_data_list` from `converter.torch_geometric_exporter` and
prints a short summary.
"""
import os
import sys

from python_src.converter.torch_geometric_exporter import load_pt_to_pyg_data_list


def main():
    # compute repo root from this file location (python_src/converter -> repo root)
    this_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(this_dir))
    pt_rel = os.path.join('Results', 'Paths', 'REFINE', 'MUTAG', 'MUTAG_edit_paths.pt')
    pt_path = os.path.join(repo_root, pt_rel)

    if not os.path.isfile(pt_path):
        print(f"PT file not found: {pt_path}")
        return 2

    try:
        data_list = load_pt_to_pyg_data_list(pt_path)
    except Exception as e:
        print(f"Error loading PT file: {e}")
        raise

    print(f"Loaded {len(data_list)} Data objects from: {pt_path}")

    # summarise start->end pairs
    pairs = {}
    for d in data_list:
        s = getattr(d, 'edit_path_start', None)
        e = getattr(d, 'edit_path_end', None)
        key = (str(s), str(e))
        pairs[key] = pairs.get(key, 0) + 1

    print("Start->End pairs (count):")
    for (s, e), c in sorted(pairs.items()):
        print(f"  {s} -> {e} : {c}")

    # show some samples
    for i, d in enumerate(data_list[:5]):
        print(f"Sample {i}: bgf_name={getattr(d, 'bgf_name', None)} bgf_name_parts={getattr(d, 'bgf_name_parts', None)} num_nodes={getattr(d, 'num_nodes', None)}")

    return 0


if __name__ == '__main__':
    sys.exit(main())

