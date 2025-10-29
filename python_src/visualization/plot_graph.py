import argparse
import os
from typing import Optional

from networkx.drawing.nx_agraph import pygraphviz_layout

try:
    import torch
    from torch_geometric.data import InMemoryDataset
    from torch_geometric.utils import to_networkx
except Exception as e:
    raise ImportError("This script requires torch and torch_geometric. Install them before running: pip install torch torch-geometric") from e

import networkx as nx
import matplotlib.pyplot as plt


class _LoadedInMemoryDataset(InMemoryDataset):
    """Tiny wrapper that lets us load a processed .pt file and use the
    InMemoryDataset interface (dataset[idx]) to obtain Data objects.

    We only implement the minimal properties required by torch-geometric.
    """
    def __init__(self, processed_path: str):
        self._processed_path = os.path.abspath(processed_path)
        # root is two directories above processed file when following
        # <root>/processed/data.pt, but we keep it flexible.
        root = os.path.dirname(os.path.dirname(self._processed_path))
        super().__init__(root)
        # load the processed (data, slices) tuple saved by torch.save
        self.data, self.slices = torch.load(self._processed_path)

    @property
    def raw_file_names(self):
        return []

    @property
    def processed_file_names(self):
        return [os.path.basename(self._processed_path)]


def find_processed_pt(path: str) -> Optional[str]:
    """Resolve a user-provided path to a processed .pt file.

    Accepts either: (1) a direct path to a .pt file, (2) a directory that
    contains a processed/data.pt, or (3) a directory that already contains
    data.pt.
    """
    if os.path.isfile(path) and path.endswith('.pt'):
        return os.path.abspath(path)
    if os.path.isdir(path):
        cand = os.path.join(path, 'processed', 'data.pt')
        if os.path.isfile(cand):
            return os.path.abspath(cand)
        cand2 = os.path.join(path, 'data.pt')
        if os.path.isfile(cand2):
            return os.path.abspath(cand2)
    return None


def load_data_by_index(processed_pt: str, idx: int):
    ds = _LoadedInMemoryDataset(processed_pt)
    if idx < 0 or idx >= len(ds):
        raise IndexError(f"Index {idx} out of range (0..{len(ds)-1})")
    return ds[idx]


def find_index_by_bgf_name(processed_pt: str, bgf_name: str) -> int:
    ds = _LoadedInMemoryDataset(processed_pt)
    # Search datasets for the bgf_name attribute on the Data objects
    for i in range(len(ds)):
        d = ds[i]
        if hasattr(d, 'bgf_name') and d.bgf_name == bgf_name:
            return i
    raise ValueError(f"No graph with bgf_name '{bgf_name}' found in {processed_pt}")


def graph_to_networkx_with_edge_features(data):
    """Convert a torch_geometric.data.Data to a networkx graph and
    prepare edge labels from edge_attr. Returns (G, edge_labels).
    """
    # ensure edge_attr exists; to_networkx will attach attributes
    G = to_networkx(data, node_attrs=None, edge_attrs=['edge_attr'])

    # Build a labels dict for matplotlib drawing
    edge_labels = {}
    for u, v, d in G.edges(data=True):
        attr = d.get('edge_attr', None)
        if attr is None:
            edge_labels[(u, v)] = ''
        else:
            try:
                # If it's a torch tensor
                if hasattr(attr, 'detach'):
                    a = attr.detach().cpu().numpy()
                else:
                    a = attr
                # handle 1-D, 2-D, and higher-dimensional edge features
                if hasattr(a, 'ndim'):
                    if a.ndim == 0:
                        edge_labels[(u, v)] = f"{float(a):.3g}"
                    elif a.ndim == 1:
                        if a.shape[0] == 1:
                            edge_labels[(u, v)] = f"{float(a[0]):.3g}"
                        elif a.shape[0] <= 4:
                            edge_labels[(u, v)] = '[' + ','.join(f"{float(x):.3g}" for x in a) + ']'
                        else:
                            edge_labels[(u, v)] = '[' + ','.join(f"{float(x):.3g}" for x in a[:6]) + '...]'
                    elif a.ndim == 2:
                        # flatten and treat as vector
                        flat = a.flatten()
                        if flat.shape[0] == 1:
                            edge_labels[(u, v)] = f"{float(flat[0]):.3g}"
                        elif flat.shape[0] <= 4:
                            edge_labels[(u, v)] = '[' + ','.join(f"{float(x):.3g}" for x in flat) + ']'
                        else:
                            edge_labels[(u, v)] = '[' + ','.join(f"{float(x):.3g}" for x in flat[:6]) + '...]'
                    else:
                        # fallback for higher dimensions
                        flat = a.flatten()
                        edge_labels[(u, v)] = '[' + ','.join(f"{float(x):.3g}" for x in flat[:6]) + '...]'
                else:
                    # fallback for non-numeric types
                    edge_labels[(u, v)] = str(a)
            except Exception:
                edge_labels[(u, v)] = str(attr)
    return G, edge_labels


def plot_graph(data, title: Optional[str] = None, show_node_labels: bool = True, output: Optional[str] = None):
    # Convert to networkx and prepare labels
    G, edge_labels = graph_to_networkx_with_edge_features(data)

    # Determine node positions: if node features exist and are 2-d, use them
    pos = None
    if hasattr(data, 'x') and data.x is not None:
        try:
            x = data.x.detach().cpu().numpy()
            if x.shape[1] == 2:
                pos = {i: (float(x[i, 0]), float(x[i, 1])) for i in range(x.shape[0])}
        except Exception:
            pos = None
    if pos is None:
        # Use Kamada-Kawai layout (often called "kawai/"kawaii" by typo)
        # It typically produces clearer layouts for small-to-medium graphs.
        try:
            pos = pygraphviz_layout(G, prog='neato')
        except Exception:
            # Fallback to spring layout if kamada_kawai fails for some graph
            pos = nx.spring_layout(G)

    # Prepare node labels from attributes if requested
    node_labels = None
    if show_node_labels:
        try:
            if hasattr(data, 'x') and data.x is not None:
                x = data.x
                # handle 1-D or 2-D numeric arrays
                if x.ndim == 1:
                    node_labels = {i: f"{i}: {float(x[i]):.3g}" for i in range(x.shape[0])}
                elif x.ndim == 2 and x.shape[1] == 1:
                    node_labels = {i: f"{i}: {float(x[i,0]):.3g}" for i in range(x.shape[0])}
                elif x.ndim == 2 and x.shape[1] <= 4:
                    node_labels = {i: f"{i}: [" + ','.join(f"{float(v):.3g}" for v in x[i].flatten()) + "]" for i in range(x.shape[0])}
                else:
                    try:
                        node_labels = {i: f"{i}: [" + ','.join(f"{float(v):.3g}" for v in x[i].flatten()[:6]) + "]" for i in range(x.shape[0])}
                    except Exception:
                        node_labels = {i: str(i) for i in range(G.number_of_nodes())}
            else:
                node_labels = {i: str(i) for i in range(G.number_of_nodes())}
        except Exception:
            node_labels = {i: str(i) for i in range(G.number_of_nodes())}

    plt.figure(figsize=(8, 6))
    # draw nodes and edges
    nx.draw_networkx_nodes(G, pos, node_color='lightblue')
    nx.draw_networkx_edges(G, pos, edge_color='gray')
    # draw node labels if prepared
    if node_labels is not None:
        nx.draw_networkx_labels(G, pos, labels=node_labels, font_size=8)

    # draw edge labels if any
    if edge_labels:
        nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_size=8)

    if title:
        # Add number of nodes and edges to the plot title
        num_nodes = G.number_of_nodes()
        num_edges = G.number_of_edges()
        plt.title(f"{title} | Nodes: {num_nodes}, Edges: {num_edges}")
    plt.tight_layout()

    if output:
        # If output looks like a directory, create a filename; otherwise use it as file path
        out_path = output
        graph_idx = None
        if title and title.startswith("Graph index "):
            try:
                graph_idx = int(title.split("Graph index ")[1].split()[0])
            except Exception:
                graph_idx = None
        elif title and "(index " in title:
            try:
                graph_idx = int(title.split("(index ")[1].split(")")[0])
            except Exception:
                graph_idx = None
        # Compose filename with graph index if available
        if os.path.isdir(output) or output.endswith(os.path.sep):
            fname = "graph_plot"
            if graph_idx is not None:
                fname += f"_idx{graph_idx}"
            fname += ".png"
            out_path = os.path.join(output, fname)
        elif not (output.lower().endswith('.png') or output.lower().endswith('.jpg') or output.lower().endswith('.pdf')):
            # if user passed e.g. '/tmp/out' without extension, add .png and index
            out_path = output
            if graph_idx is not None:
                out_path += f"_idx{graph_idx}"
            out_path += ".png"
        plt.savefig(out_path, dpi=300)
        print(f"Saved plot to: {out_path}")
    else:
        plt.show()


def plot_edit_path(graphs, edit_ops, output=None, highlight_colors=None, show_labels=True, one_fig_per_step = False):
    """
    Visualize an edit path between two graphs.
    Args:
        graphs: list of torch_geometric.data.Data objects representing the graph at each step.
        edit_ops: list of dicts or strings describing the edit operation at each step.
        output: if provided, save the figure(s) to this path (as PNG or PDF).
        highlight_colors: dict mapping operation types to colors (optional).
        show_labels: whether to show node labels.
        one_fig_per_step: plot one figure per step or all steps as subplots in one figure
    """
    import matplotlib.pyplot as plt
    import networkx as nx
    from math import ceil, sqrt

    n_steps = len(graphs)
    if highlight_colors is None:
        highlight_colors = {
            'node_insert': 'lime',
            'node_delete': 'red',
            'node_subst': 'orange',
            'edge_insert': 'cyan',
            'edge_delete': 'magenta',
            'edge_subst': 'yellow',
            'none': 'gray',
        }

    # helper to parse an edit operation into a unified dict
    def _parse_edit_op(op):
        """Return dict with keys: type (str), nodes (set), edges (set of (u,v))."""
        res = {'type': 'none', 'nodes': set(), 'edges': set()}
        if op is None:
            return res
        # dict-like
        if isinstance(op, dict):
            # try common keys
            t = op.get('type') or op.get('op') or op.get('action') or 'none'
            res['type'] = str(t)
            # nodes: accept 'node', 'nodes'
            nodes = op.get('nodes') or op.get('node')
            if nodes is not None:
                if isinstance(nodes, (list, tuple, set)):
                    res['nodes'] = set(int(x) for x in nodes)
                else:
                    try:
                        res['nodes'] = {int(nodes)}
                    except Exception:
                        pass
            # edges: accept 'edge', 'edges' as pairs
            edges = op.get('edges') or op.get('edge')
            if edges is not None:
                e_set = set()
                if isinstance(edges, (list, tuple, set)):
                    for e in edges:
                        try:
                            u, v = e
                            e_set.add((int(u), int(v)))
                        except Exception:
                            continue
                else:
                    # single pair
                    try:
                        u, v = edges
                        e_set.add((int(u), int(v)))
                    except Exception:
                        pass
                res['edges'] = e_set
            return res

        # string-like: try formats 'optype', 'optype:payload' where payload can be '5' or '(1,2)'
        if isinstance(op, str):
            s = op.strip()
            if ':' in s:
                typ, payload = s.split(':', 1)
                typ = typ.strip()
                payload = payload.strip()
                res['type'] = typ
                # node id
                if payload.startswith('(') and payload.endswith(')'):
                    payload = payload[1:-1]
                if ',' in payload:
                    parts = [p.strip() for p in payload.split(',')]
                    if len(parts) == 2:
                        try:
                            u = int(parts[0]); v = int(parts[1])
                            res['edges'].add((u, v))
                        except Exception:
                            pass
                else:
                    try:
                        res['nodes'].add(int(payload))
                    except Exception:
                        pass
            else:
                res['type'] = s
            return res

        # fallback
        return res

    # small helper that draws a single graph on the provided Axes and applies highlights
    def _draw_graph_on_ax(data, ax, title=None, show_node_labels=True, op=None):
        G, edge_labels = graph_to_networkx_with_edge_features(data)

        # determine positions (try node features first, else layout)
        pos = None
        if hasattr(data, 'x') and data.x is not None:
            try:
                x = data.x.detach().cpu().numpy()
                if x.ndim == 2 and x.shape[1] == 2:
                    pos = {i: (float(x[i, 0]), float(x[i, 1])) for i in range(x.shape[0])}
            except Exception:
                pos = None
        if pos is None:
            try:
                pos = pygraphviz_layout(G, prog='neato', root=0, args=' -Gmode="KK" -Nmaxiter=10000 ')
            except Exception:
                print("pygraphviz_layout failed, falling back to spring_layout")
                pos = nx.spring_layout(G)

        # prepare node labels
        node_labels = None
        if show_node_labels:
            try:
                if hasattr(data, 'x') and data.x is not None:
                    x = data.x
                    if x.ndim == 1:
                        node_labels = {i: f"{i}: {float(x[i]):.3g}" for i in range(x.shape[0])}
                    elif x.ndim == 2 and x.shape[1] == 1:
                        node_labels = {i: f"{i}: {float(x[i,0]):.3g}" for i in range(x.shape[0])}
                    elif x.ndim == 2 and x.shape[1] <= 4:
                        node_labels = {i: f"{i}: [" + ','.join(f"{float(v):.3g}" for v in x[i].flatten()) + "]" for i in range(x.shape[0])}
                    else:
                        try:
                            node_labels = {i: f"{i}: [" + ','.join(f"{float(v):.3g}" for v in x[i].flatten()[:6]) + "]" for i in range(x.shape[0])}
                        except Exception:
                            node_labels = {i: str(i) for i in range(G.number_of_nodes())}
                else:
                    node_labels = {i: str(i) for i in range(G.number_of_nodes())}
            except Exception:
                node_labels = {i: str(i) for i in range(G.number_of_nodes())}

        # compute highlight sets from op
        parsed = _parse_edit_op(op)
        op_type = parsed.get('type', 'none')
        highlight_node_set = parsed.get('nodes', set())
        highlight_edge_set = parsed.get('edges', set())
        # normalize edge direction: networkx undirected edges may be (u,v) or (v,u)
        normalized_edge_set = set()
        for u, v in highlight_edge_set:
            normalized_edge_set.add((u, v))
            normalized_edge_set.add((v, u))

        # choose color for op_type fallback
        op_color = highlight_colors.get(op_type, highlight_colors.get('none', 'gray'))

        # draw on the provided axis: draw defaults first, then overlay highlighted nodes/edges
        ax.clear()
        # draw all nodes with default color
        nx.draw_networkx_nodes(G, pos, node_color='lightblue', ax=ax)
        # draw highlighted nodes on top (if any and present in G)
        try:
            highlighted_nodes_present = [n for n in highlight_node_set if n in G.nodes()]
            if highlighted_nodes_present:
                nx.draw_networkx_nodes(G, pos, nodelist=highlighted_nodes_present, node_color=op_color, ax=ax)
        except Exception:
            pass

        # draw all edges with default color
        nx.draw_networkx_edges(G, pos, edge_color='gray', ax=ax)
        # draw highlighted edges thicker and colored
        try:
            highlighted_edges_present = [e for e in G.edges() if (e[0], e[1]) in normalized_edge_set or (e[1], e[0]) in normalized_edge_set]
            if highlighted_edges_present:
                nx.draw_networkx_edges(G, pos, edgelist=highlighted_edges_present, edge_color=op_color, width=2.0, ax=ax)
        except Exception:
            pass

        if node_labels is not None:
            nx.draw_networkx_labels(G, pos, labels=node_labels, font_size=8, ax=ax)
        if edge_labels:
            nx.draw_networkx_edge_labels(G, pos, edge_labels=edge_labels, font_size=8, ax=ax)

        if title:
            ax.set_title(f"{title}")
        ax.set_axis_off()

    if n_steps == 0:
        raise ValueError("No graphs provided to plot_edit_path")

    if one_fig_per_step:
        # produce one file (or show) per step using the internal drawer so we can apply highlights
        for step in range(n_steps):
            data = graphs[step]

            op = edit_ops[step] if step < len(edit_ops) else None
            title = None
            if step > 0:
                title = f"Step {step}" + f": {edit_ops[step-1]}"

            fig, ax = plt.subplots(figsize=(8, 6))
            _draw_graph_on_ax(data, ax, title=title, show_node_labels=show_labels, op=op)

            single_output = None
            if output:
                if os.path.isdir(output) or output.endswith(os.path.sep):
                    base = output
                    fname = f"editpath_step{step}.png"
                    single_output = os.path.join(base, fname)
                else:
                    root, ext = os.path.splitext(output)
                    if ext == '':
                        ext = '.png'
                    single_output = f"{root}_step{step}{ext}"
                fig.savefig(single_output, dpi=300)
                print(f"Saved edit path step to: {single_output}")
                plt.close(fig)
            else:
                plt.show()
    else:
        # single figure with subplots for each step
        cols = int(ceil(sqrt(n_steps)))
        rows = int(ceil(n_steps / cols))
        fig, axes = plt.subplots(rows, cols, figsize=(4 * cols, 3 * rows))
        # flatten axes
        if isinstance(axes, (list, tuple)):
            ax_list = [a for a in axes]
        else:
            try:
                ax_list = axes.flatten().tolist()
            except Exception:
                ax_list = [axes]

        for i in range(rows * cols):
            if i < n_steps:
                ax = ax_list[i]
                data = graphs[i]
                op = edit_ops[i] if i < len(edit_ops) else None
                title = None
                if i > 0:
                    title = f"Step {i}" + f": {edit_ops[i-1]}"
                _draw_graph_on_ax(data, ax, title=title, show_node_labels=show_labels, op=op)
            else:
                ax = ax_list[i]
                ax.set_visible(False)

        plt.tight_layout()
        if output:
            out_path = output
            if os.path.isdir(output) or output.endswith(os.path.sep):
                out_path = os.path.join(output, 'editpath_subplots.png')
            else:
                root, ext = os.path.splitext(output)
                if ext == '':
                    out_path = root + '.png'
            fig.savefig(out_path, dpi=300)
            print(f"Saved edit path subplots to: {out_path}")
            plt.close(fig)
        else:
            plt.show()


def main():
    parser = argparse.ArgumentParser(description="Load a torch-geometric processed .pt file and plot a single graph (by index or bgf name) using networkx. Edge features are shown as edge labels.")
    # positional argument for copy-pastable usage
    parser.add_argument('--path', help='Path to processed .pt file or dataset root containing processed/data.pt')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--index', type=int, help='Graph index (0-based) to plot')
    group.add_argument('--name', type=str, help='bgf_name of the graph to plot (searches the dataset)')
    parser.add_argument('--output', '-o', help='If provided, save the figure to this path (e.g. out.png)')
    parser.add_argument('--no-node-labels', dest='node_labels', action='store_false', help='Do not draw node labels')
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

    plot_graph(data, title=title, show_node_labels=args.node_labels, output=args.output)


if __name__ == '__main__':
    main()
