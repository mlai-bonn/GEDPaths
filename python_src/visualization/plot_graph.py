import argparse

# Import plotting and dataset helpers from the extracted module
try:
    # When used as a package (python -m python_src.visualization.plot_graph)
    from .visualization_functions import find_processed_pt, load_data_by_index, find_index_by_bgf_name, plot_graph
except Exception:
    # When executed directly as a script (python plot_graph.py), allow local import
    from visualization_functions import find_processed_pt, load_data_by_index, find_index_by_bgf_name, plot_graph


def main():
    parser = argparse.ArgumentParser(description="Load a torch-geometric processed .pt file and plot a single graph (by index or bgf name) using networkx. Edge features are shown as edge labels.")
    # positional argument for copy-pastable usage
    parser.add_argument('--path', help='Path to processed .pt file or dataset root containing processed/data.pt')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--index', type=int, help='Graph index (0-based) to plot')
    group.add_argument('--name', type=str, help='bgf_name of the graph to plot (searches the dataset)')
    parser.add_argument('--output', '-o', help='If provided, save the figure to this path (e.g. out.png)')
    parser.add_argument('--no-node-labels', dest='node_labels', action='store_false', help='Do not draw node labels')
    parser.add_argument('--no-color-nodes-by-label', dest='color_nodes_by_label', action='store_false', help='Do not color nodes by label; show labels inside nodes instead')
    args = parser.parse_args()

    # resolve processed path
    processed = find_processed_pt(args.path)
    if processed is None:
        raise FileNotFoundError(f"Could not find a processed .pt file at '{args.path}'. Provide a path to a .pt file or a dataset root containing processed/data.pt")

    if args.index is not None:
        data = load_data_by_index(processed, args.index)
        title = f"Graph index {args.index}"
    else:
        idx = find_index_by_bgf_name(processed, args.name)
        data = load_data_by_index(processed, idx)
        title = f"Graph '{args.name}' (index {idx})"

    plot_graph(data, title=title, show_node_labels=args.node_labels, output=args.output, color_nodes_by_label=args.color_nodes_by_label)


if __name__ == '__main__':
    main()
