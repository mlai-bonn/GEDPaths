from os.path import dirname
from python_src.converter.torch_geometric_exporter import BGFInMemoryDataset

if __name__ == "__main__":
    strategy = "Rnd_d-IsoN"
    strategy = "Rnd"
    bgf_path = f"Results/Paths_{strategy}/F2/MUTAG/MUTAG_edit_paths.bgf"
    # Use the directory containing the bgf as the dataset root so the processed file
    # will be written to <root>/processed/data.pt
    root_dir = dirname(bgf_path) or "."

    ds = BGFInMemoryDataset(root=root_dir, path=bgf_path)
    print(f"Processed dataset stored at: {ds.processed_paths[0]}")
    try:
        print(f"Dataset length (graphs): {len(ds)}")
    except Exception:
        # Fallback: if len() isn't available, try to infer from slices
        if hasattr(ds, "slices") and isinstance(ds.slices, dict):
            # slices typically contains 'x' or 'edge_index' keys mapping to tensors
            any_slice = next(iter(ds.slices.values()))
            # number of examples equals length of the first slice dimension
            print(f"Dataset length (graphs, inferred): {len(any_slice)}")
        else:
            print("Dataset length: unknown")
