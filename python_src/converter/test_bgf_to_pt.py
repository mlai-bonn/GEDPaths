import sys
import os
from python_src.converter.torch_geometric_exporter import load_bgf_two_pass_to_pyg_data_list_numpy
import torch

if __name__ == "__main__":
    bgf_path = "Results/Paths/REFINE/MUTAG/MUTAG_edit_paths.bgf"
    pt_path = "Results/Paths/REFINE/MUTAG/MUTAG_edit_paths.pt"
    data_list = load_bgf_two_pass_to_pyg_data_list_numpy(bgf_path)
    torch.save(data_list, pt_path)
    print(f"Converted {bgf_path} to {pt_path} with {len(data_list)} graphs.")

