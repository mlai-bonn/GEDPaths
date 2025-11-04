from os.path import dirname
import argparse


def main(strategy: str) -> None:
    """Convert a BGF file to a torch_geometric InMemoryDataset using the given strategy name.

    The script expects the BGF file to be at: Results/Paths_{strategy}/F2/MUTAG/MUTAG_edit_paths.bgf
    """
    # Ensure the project root is importable as a package root so `python_src` can be found
    import os
    import sys
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if project_root not in sys.path:
        sys.path.insert(0, project_root)

    # import the dataset wrapper lazily (avoids importing heavy deps on --help)
    from python_src.converter.torch_geometric_exporter import BGFInMemoryDataset

    bgf_path = f"Results/Paths_{strategy}/F2/MUTAG/MUTAG_edit_paths.bgf"
    print(f"Using strategy: {strategy}")
    print(f"Looking for BGF at: {bgf_path}")

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert a BGF file to torch_geometric .pt using a generating-path strategy name"
    )
    parser.add_argument(
        "-s",
        "--strategy",
        dest="strategy",
        default="Rnd_d-IsoN",
        help=(
            "Generating path strategy name used inside Results/Paths_{strategy}/. "
            "Default: 'Rnd_d-IsoN'."
        ),
    )
    args = parser.parse_args()
    main(args.strategy)
