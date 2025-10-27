import sys
import os
from os.path import dirname

from python_src.converter.GEDPathsInMemory import GEDPathsInMemoryDataset
from python_src.converter.torch_geometric_exporter import load_bgf_two_pass_to_pyg_data_list_numpy, BGFInMemoryDataset
import torch

if __name__ == "__main__":
    bgf_path = "Results/Paths/F2/MUTAG/MUTAG_edit_paths.bgf"
    edit_operation_path = "Results/Paths/F2/MUTAG/MUTAG_edit_path_data.txt"
    # Use the directory containing the bgf as the dataset root so the processed file
    # will be written to <root>/processed/data.pt
    root_dir = dirname(bgf_path) or "."

    ds = BGFInMemoryDataset(root=root_dir, path=bgf_path)
    print(f"Loaded dataset from: {ds.processed_paths[0]}")

    ds_ged = GEDPathsInMemoryDataset(root=root_dir, path=bgf_path, edit_path_data=edit_operation_path)
    print(f"Processed dataset stored at: {ds.processed_paths[0]}")

