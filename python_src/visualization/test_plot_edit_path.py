import os
import torch
from torch_geometric.data import InMemoryDataset
from plot_graph import plot_edit_path, find_processed_pt
from python_src.converter.GEDPathsInMemory import GEDPathsInMemoryDataset

# Paths
root_dir = "../../Results/Paths/F2/MUTAG/"
processed_dir = "../../Results/Paths/F2/MUTAG/processed"
edit_path_file = "../../Results/Paths/F2/MUTAG/MUTAG_edit_path_data.txt"

# Load processed dataset
processed_pt = find_processed_pt(processed_dir)
assert processed_pt is not None, f"Processed .pt file not found in {processed_dir}"
ds = GEDPathsInMemoryDataset(root_dir, path=processed_pt, edit_path_data=edit_path_file)

start = 0
end = 6
path_graphs = ds.get_path_graphs(start, end)
tmp_edit_operations = ds.get_path_operations(start, end)
edit_operations = []
for op in tmp_edit_operations:
    edit_operations.append(op.operation["raw"])  # extract raw dict

plot_edit_path(path_graphs, edit_operations, output=f"edit_path_{start}_{end}.png")
print(f"Edit path plot saved to edit_path_{start}_{end}.png")

