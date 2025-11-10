import argparse
import os
from typing import Optional
import ast

from networkx.drawing.nx_agraph import pygraphviz_layout

try:
    import torch
    from torch_geometric.data import InMemoryDataset
    from torch_geometric.utils import to_networkx
except Exception as e:
    raise ImportError("This script requires torch and torch_geometric. Install them before running: pip install torch torch-geometric") from e

import networkx as nx
import matplotlib.pyplot as plt
import matplotlib as mpl


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
        # load the processed file in a way compatible with multiple PyG versions
        out = torch.load(self._processed_path, weights_only=False)
        if isinstance(out, tuple):
            if len(out) == 2:
                self.data, self.slices = out
                self.sizes = {}
            elif len(out) == 3:
                self.data, self.slices, self.sizes = out
            else:
                # len >= 4: (data, slices, sizes, data_cls, ...)
                self.data, self.slices, self.sizes = out[0], out[1], out[2]
        else:
            # Fallback: single-object save
            self.data = out
            self.slices = {}
            self.sizes = {}

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


def graph_to_networkx_with_edge_features(data, one_hot_encoding: bool = True):
    """Convert a torch_geometric.data.Data to a networkx graph and
    prepare edge labels from edge_attr. Returns (G, edge_labels).
    """
    # ensure edge_attr exists; to_networkx will attach attributes
    G = to_networkx(data, node_attrs=['primary_node_labels'], edge_attrs=['edge_attributes'], to_undirected=True)

    # Force undirected graph semantics for plotting/analysis
    try:
        G = nx.Graph(G)
    except Exception:
        # fallback: if conversion fails, keep original
        pass

    # Build a labels dict for matplotlib drawing
    edge_labels = {}
    for u, v, d in G.edges(data=True):
        attr = d.get('edge_attributes', None)
        if attr is None:
            edge_labels[(u, v)] = ''
        else:
            try:
                if one_hot_encoding:
                    attr = torch.argmax(torch.tensor(attr)).item()
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


def _draw_colored_edges(G, pos, edge_labels, ax, palette, edge_width=1.0):
    """Draw edges using `palette`.
    - Draw base light gray edges first.
    - If an edge's label parses as an int, draw that edge thick and colored by palette[int % len(palette)].
    - Otherwise draw the label text at the edge midpoint, colored by a categorical mapping.
    """
    # base faint edges
    try:
        nx.draw_networkx_edges(G, pos, edge_color='lightgray', edge_width=edge_width, ax=ax)
    except Exception:
        pass

    edge_list_colored = []
    edge_colors = []
    edge_text_items = []
    # iterate over actual graph edges to ensure consistent order and presence
    for u, v in G.edges():
        # look up label in either orientation
        lbl = edge_labels.get((u, v), edge_labels.get((v, u), None))
        if lbl is None:
            continue
        s = str(lbl)
        # Try to parse the label safely. It can be a scalar string like '1', or a list-like '[1,2]'.
        parsed_int = None
        try:
            val = ast.literal_eval(s)
            # if it's a sequence, take first element
            if isinstance(val, (list, tuple)) and len(val) > 0:
                candidate = val[0]
            else:
                candidate = val
            parsed_int = int(float(candidate))
        except Exception:
            # fallback: try to parse directly from the string as a number
            try:
                parsed_int = int(float(s))
            except Exception:
                parsed_int = None

        if parsed_int is not None:
            edge_list_colored.append((u, v))
            edge_colors.append(palette[parsed_int % len(palette)])
        else:
            try:
                midx = (pos[u][0] + pos[v][0]) / 2.0
                midy = (pos[u][1] + pos[v][1]) / 2.0
                edge_text_items.append((midx, midy, s))
            except Exception:
                continue

    if edge_list_colored:
        try:
            edge_widths = [1 for _ in edge_list_colored]
            # double edge width for if type is DELETE or relabel
            nx.draw_networkx_edges(G, pos, edgelist=edge_list_colored, edge_color=edge_colors, width=edge_width, ax=ax)
        except Exception:
            pass
        # When integer-labeled edges are present and colored, omit drawing any edge label text
        return

    if edge_text_items:
        unique_texts = sorted(list({t for (_, _, t) in edge_text_items}))
        mapping = {val: palette[i % len(palette)] for i, val in enumerate(unique_texts)}
        for midx, midy, text in edge_text_items:
            color = mapping.get(text, '#444444')
            try:
                ax.text(midx, midy, text, fontsize=8, color=color, ha='center', va='center', bbox=dict(facecolor='white', edgecolor='none', alpha=0.7))
            except Exception:
                pass


def plot_graph(data, title: Optional[str] = None, show_node_labels: bool = True, output: Optional[str] = None, color_nodes_by_label: bool = True):
    # Convert to networkx and prepare labels
    G, edge_labels = graph_to_networkx_with_edge_features(data)
    standard_edge_width = 2.0
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

    # Prepare node labels from attributes if requested.
    # We remove node ids from the inside labels (they will be shown outside).
    node_labels = None
    if show_node_labels:
        try:
            if hasattr(data, 'x') and data.x is not None:
                x = data.x
                # handle 1-D or 2-D numeric arrays -> only show the attribute values inside the node
                if x.ndim == 1:
                    node_labels = {i: f"{float(x[i]):.3g}" for i in range(x.shape[0])}
                elif x.ndim == 2 and x.shape[1] == 1:
                    node_labels = {i: f"{float(x[i,0]):.3g}" for i in range(x.shape[0])}
                elif x.ndim == 2 and x.shape[1] <= 4:
                    node_labels = {i: '[' + ','.join(f"{float(v):.3g}" for v in x[i].flatten()) + ']' for i in range(x.shape[0])}
                else:
                    try:
                        node_labels = {i: '[' + ','.join(f"{float(v):.3g}" for v in x[i].flatten()[:6]) + ']' for i in range(x.shape[0])}
                    except Exception:
                        node_labels = None
            else:
                # No node attributes -> do not draw any label inside the node
                node_labels = None
        except Exception:
            node_labels = None

    # draw nodes and edges
    # If requested, color nodes by their integer labels (0,1,2,...) using a fixed tab20 palette.
    # If node labels are not integer-like, fall back to categorical mapping.
    colored_legend = None
    palette = [mpl.colors.to_hex(c) for c in plt.get_cmap('tab20').colors]
    if color_nodes_by_label and node_labels is not None:
        node_order = list(G.nodes())
        vals = [node_labels.get(n) for n in node_order]
        # try parse as integers
        int_vals = []
        all_int = True
        for v in vals:
            try:
                iv = int(float(v))
                int_vals.append(iv)
            except Exception:
                all_int = False
                break

        if all_int and len(int_vals) > 0:
            # direct integer->color mapping (value modulo palette size)
            colors_hex = [palette[iv % len(palette)] for iv in int_vals]
            nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex)
            # legend: show only unique integer categories present
            unique_ints = sorted(set(int_vals))
            legend_items = [(str(i), palette[i % len(palette)]) for i in unique_ints]
            colored_legend = ('categorical', legend_items)
        else:
            # fallback categorical mapping
            unique = sorted(list(dict.fromkeys(vals)))
            mapping = {val: palette[i % len(palette)] for i, val in enumerate(unique)}
            colors_hex = [mapping.get(v, '#cccccc') for v in vals]
            nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex)
            legend_items = [(val, mapping[val]) for val in unique]
            colored_legend = ('categorical', legend_items)
    else:
        nx.draw_networkx_nodes(G, pos, node_color='lightblue')

    nx.draw_networkx_edges(G, pos, edge_color='gray')
    # draw edge colors/labels using helper (colors and optional labels)
    _draw_colored_edges(G, pos, edge_labels, plt.gca(), palette, edge_width=standard_edge_width)

    # draw labels: inside labels (bold black) show node attributes; outside ids (bold red) show node indices
    if node_labels is not None and (not color_nodes_by_label):
        # draw the label inside the node on top of the node marker (bold, black)
        nx.draw_networkx_labels(G, pos, labels=node_labels, font_size=8, font_color='black', font_weight='bold')

    # always draw node ids outside the nodes (bold, red)
    try:
        xs = [p[0] for p in pos.values()]
        ys = [p[1] for p in pos.values()]
        dx = (max(xs) - min(xs)) if xs else 1.0
        dy = (max(ys) - min(ys)) if ys else 1.0
        off = 0.03 * max(dx, dy, 1.0)
        pos_ids = {n: (pos[n][0] + off, pos[n][1] + off) for n in G.nodes()}
        id_labels = {n: str(n) for n in G.nodes()}
        nx.draw_networkx_labels(G, pos_ids, labels=id_labels, font_size=8, font_color='red', font_weight='bold')
    except Exception:
        pass

    # if we prepared a legend for colored nodes (or edges), add it on the right
    if colored_legend is not None:
        kind, payload = colored_legend
        fig = plt.gcf()
        if kind == 'categorical':
            import matplotlib.patches as mpatches
            patches = [mpatches.Patch(color=col, label=str(lbl)) for lbl, col in payload]
            plt.gca().legend(handles=patches, bbox_to_anchor=(1.02, 1), loc='upper left', borderaxespad=0.)

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


def plot_edit_path(graphs, edit_ops, output=None, highlight_colors=None, show_labels=True, one_fig_per_step = False, color_nodes_by_label: bool = True):
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
            'delete': 'red',
            'Delete': 'red',
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

        # list-like: [object, ids or id, operation type]
        if isinstance(op, list):
            # expecting 3 elements
            if len(op) == 3:
                obj = op[0]
                ids = op[1]
                typ = op[2]
                res['type'] = str(typ)
                if obj in ('node', 'nodes', 'NODE'):
                    if isinstance(ids, (list, tuple, set)):
                        res['nodes'] = set(int(x) for x in ids)
                    else:
                        try:
                            res['nodes'] = {int(ids)}
                        except Exception:
                            pass
                elif obj in ('edge', 'edges', 'EDGE'):
                    e_set = set()
                    # format is id1--id2
                    ids = ids.split('--')
                    try:
                        e_set.add((int(ids[0]), int(ids[1])))
                    except Exception:
                        pass
                    res['edges'] = e_set
                return res

        # fallback
        return res

    # New helper: format an edit op into a human-readable title like
    # "Insert Node 5", "Delete Edge 1 - 2", "Relabel Node 3" or "Initial".
    def _format_edit_op(op):
        parsed = _parse_edit_op(op)
        typ = str(parsed.get('type', 'none')).lower()

        # determine action
        action = None
        if any(k in typ for k in ('insert', 'add', 'create')):
            action = 'Insert'
        elif any(k in typ for k in ('delete', 'del', 'remove')):
            action = 'Delete'
        elif any(k in typ for k in ('sub', 'relabel', 'replace', 'subst')):
            action = 'Relabel'
        elif typ and typ != 'none':
            # fallback: capitalize first token
            action = typ.replace('_', ' ').capitalize()
        else:
            action = None

        # determine target (node(s) or edge(s))
        nodes = parsed.get('nodes', set())
        edges = parsed.get('edges', set())

        if nodes and not edges:
            # one or more nodes
            node_list = sorted(nodes)
            if len(node_list) == 1:
                target = f"Node {node_list[0]}"
            else:
                target = "Nodes " + ",".join(str(n) for n in node_list)
        elif edges:
            # show first edge or comma-separated list
            edge_list = sorted(list(edges))
            # pick representative: prefer a single pair
            rep = edge_list[0]
            try:
                u, v = rep
                target = f"Edge {u} - {v}"
            except Exception:
                target = "Edge"
        else:
            # no explicit nodes/edges provided; try infer from textual type
            if 'node' in typ:
                target = 'Node'
            elif 'edge' in typ:
                target = 'Edge'
            else:
                target = ''

        if action is None and not target:
            return 'Initial'

        if action and target:
            return f"{action} {target}"
        elif action:
            return action
        elif target:
            return target
        else:
            return 'Initial'

    # small helper that draws a single graph on the provided Axes and applies highlights
    # accepts an optional `color_mapping` which is either ('numeric', bins, palette) or ('categorical', mapping_dict, palette)
    def _draw_graph_on_ax(data, ax, title=None, show_node_labels=True, op=None, show_node_ids_outside=True, color_nodes_by_label=True, color_mapping=None):
        G, edge_labels = graph_to_networkx_with_edge_features(data)
        standard_edge_width = 2.0
        standard_node_size = 100
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
                if hasattr(data, 'primary_node_labels') and data.primary_node_labels is not None:
                    x = data.primary_node_labels
                    # handle 1-D or 2-D numeric arrays -> only show the attribute values inside the node
                    if x.ndim == 1:
                        node_labels = {i: int(x[i]) for i in range(x.shape[0])}
                    elif x.ndim == 2 and x.shape[1] == 1:
                        node_labels = {i: f"{float(x[i,0]):.3g}" for i in range(x.shape[0])}
                    elif x.ndim == 2 and x.shape[1] <= 4:
                        node_labels = {i: '[' + ','.join(f"{float(v):.3g}" for v in x[i].flatten()) + ']' for i in range(x.shape[0])}
                    else:
                        try:
                            node_labels = {i: '[' + ','.join(f"{float(v):.3g}" for v in x[i].flatten()[:6]) + ']' for i in range(x.shape[0])}
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

        # draw on the provided axis: draw defaults first, then overlay highlighted nodes/edges
        ax.clear()
        colored_legend = None


        # draw network nodes borders for operations (nodes with bigger sizes)
        hex_white = mpl.colors.to_hex(mpl.colors.to_rgb('black'))
        border_colors = [hex_white for _ in G.nodes()]
        border_factor = 4
        node_sizes = [standard_node_size for _ in G.nodes()]
        if op is not None:
            if op_type in ('DELETE', 'node_delete', 'Delete'):
                if parsed.get('nodes'):
                    n = list(parsed.get('nodes'))[0]
                    border_colors[n] = mpl.colors.to_hex(mpl.colors.to_rgb('red'))
                    node_sizes[n] = int(border_factor * standard_node_size)
            elif op_type in ('RELABEL', 'node_subst', 'Relabel'):
                if parsed.get('nodes'):
                    n = list(parsed.get('nodes'))[0]
                    border_colors[n] = mpl.colors.to_hex(mpl.colors.to_rgb('black'))
                    node_sizes[n] = border_factor * standard_node_size
            elif op_type in ('INSERT', 'node_insert', 'Insert'):
                if parsed.get('edges'):
                    source, target = list(parsed.get('edges'))[0]
                    border_colors[source] = mpl.colors.to_hex(mpl.colors.to_rgb('green'))
                    node_sizes[source] = border_factor * standard_node_size
                    border_colors[target] = mpl.colors.to_hex(mpl.colors.to_rgb('green'))
                    node_sizes[target] = border_factor * standard_node_size

        nx.draw_networkx_nodes(G, pos, node_color=border_colors, node_size=node_sizes, ax=ax)

        # draw nodes colored by label if requested
        if color_nodes_by_label and node_labels is not None:
            node_order = list(G.nodes())
            vals = [node_labels.get(n) for n in node_order]
            # prepare palette
            palette_local = [mpl.colors.to_hex(c) for c in plt.get_cmap('tab20').colors]
            # If a global color_mapping is provided, honor it (categorical mapping or numeric bins)
            if color_mapping is not None:
                kind = color_mapping[0]
                if kind == 'numeric':
                    bins = color_mapping[1]; pal = color_mapping[2]
                    colors_hex = []
                    for v in vals:
                        try:
                            fv = float(v)
                            b = 0
                            for i in range(len(bins) - 1):
                                if fv >= bins[i] and fv <= bins[i + 1]:
                                    b = i; break

                            colors_hex.append(pal[b % len(pal)])
                        except Exception:
                            colors_hex.append('#cccccc')
                    nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex, node_size=standard_node_size, ax=ax)
                    # prepare legend from bins
                    legend_items = [(f"{bins[i]:.3g}–{bins[i+1]:.3g}", pal[i % len(pal)]) for i in range(len(bins)-1)]
                    colored_legend = ('categorical', legend_items)
                elif kind == 'categorical':
                    mapping = color_mapping[1]; pal = color_mapping[2]
                    colors_hex = [palette_local[v] for v in vals]
                    nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex, node_size=standard_node_size, ax=ax)
                    legend_items = [(lbl, mapping[lbl]) for lbl in mapping.keys()]
                    colored_legend = ('categorical', legend_items)
                else:
                    # unknown color_mapping form -> fallback to local integer/categorical logic below
                    color_mapping = None

            if color_mapping is None:
                # Try integer mapping: parse each label as int(float(v)). If all succeed, map directly using palette index.
                int_vals = []
                all_int = True
                for v in vals:
                    try:
                        iv = int(float(v))
                        int_vals.append(iv)
                    except Exception:
                        all_int = False
                        break

                if all_int and len(int_vals) > 0:
                    colors_hex = [palette_local[iv % len(palette_local)] for iv in int_vals]
                    nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex, node_size=standard_node_size, ax=ax)
                    unique_ints = sorted(set(int_vals))
                    legend_items = [(str(i), palette_local[i % len(palette_local)]) for i in unique_ints]
                    colored_legend = ('categorical', legend_items)
                else:
                    # fallback categorical mapping using the string values
                    unique = sorted(list(dict.fromkeys(vals)))
                    mapping = {val: palette_local[i % len(palette_local)] for i, val in enumerate(unique)}
                    colors_hex = [mapping.get(v, '#cccccc') for v in vals]
                    nx.draw_networkx_nodes(G, pos, nodelist=node_order, node_color=colors_hex, node_size=standard_node_size, ax=ax)
                    legend_items = [(val, mapping[val]) for val in unique]
                    colored_legend = ('categorical', legend_items)
        else:
            nx.draw_networkx_nodes(G, pos, node_color='lightblue', ax=ax)

        # prepare palette for tab20
        palette = [mpl.colors.to_hex(c) for c in plt.get_cmap('tab20').colors]
        edge_widths = [1 for _ in G.edges()]
        edge_colors = [mpl.colors.to_hex(mpl.colors.to_rgb('lightgray')) for _ in G.edges()]
        # double edge width for if type is DELETE or relabel
        if parsed and parsed['edges']:
            for i, (u, v) in enumerate(G.edges()):
                if (u, v) in normalized_edge_set:
                    edge_widths[i] = 5.0
                    edge_colors[i] = mpl.colors.to_hex(mpl.colors.to_rgb('red'))
        nx.draw_networkx_edges(G, pos, width=edge_widths, edge_color=edge_colors, ax=ax)
        # draw edge colors/labels using helper (colors and optional labels)
        if edge_labels:
            _draw_colored_edges(G, pos, edge_labels, ax, palette, edge_width=standard_edge_width)

        # draw labels: inside labels (bold black) show node attributes; outside ids (bold red) show node indices
        if node_labels is not None and (not color_nodes_by_label):
            # draw the label inside the node on top of the node marker (bold, black)
            nx.draw_networkx_labels(G, pos, labels=node_labels, font_size=8, font_color='black', font_weight='bold', ax=ax)

        # always draw node ids outside the nodes (bold, red)
        try:
            xs = [p[0] for p in pos.values()]
            ys = [p[1] for p in pos.values()]
            dx = (max(xs) - min(xs)) if xs else 1.0
            dy = (max(ys) - min(ys)) if ys else 1.0
            off = 0.03 * max(dx, dy, 1.0)
            pos_ids = {n: (pos[n][0] + off, pos[n][1] + off) for n in G.nodes()}
            id_labels = {n: str(n) for n in G.nodes()}
            nx.draw_networkx_labels(G, pos_ids, labels=id_labels, font_size=8, font_color='red', font_weight='bold', ax=ax)
        except Exception:
            pass

        # return legend info (kind, payload) so caller can place a single legend on the figure if desired
        # Set the axis title to the formatted operation string (user requested)
        try:
            title_text = None
            # Prefer an explicit op description if provided
            if op is not None:
                title_text = _format_edit_op(op)
            # fall back to caller-provided title
            if (title_text is None or title_text == '') and title:
                title_text = title
            if title_text is None or title_text == '':
                title_text = 'Initial'
            ax.set_title(title_text)
        except Exception:
            try:
                if title:
                    ax.set_title(str(title))
            except Exception:
                pass

        return colored_legend

    if n_steps == 0:
        raise ValueError("No graphs provided to plot_edit_path")

    # Build a global color_mapping across all graphs so colors are consistent across steps
    global_color_mapping = None
    if color_nodes_by_label:
        all_vals = []
        all_numeric = True
        numeric_list = []
        import numpy as np
        for g in graphs:
            try:
                if hasattr(g, 'x') and g.x is not None:
                    try:
                        x = g.x.detach().cpu().numpy()
                    except Exception:
                        x = np.array(g.x)
                    if hasattr(x, 'ndim') and x.ndim == 1:
                        vals_g = [f"{float(x[i]):.3g}" for i in range(x.shape[0])]
                    elif hasattr(x, 'ndim') and x.ndim == 2 and x.shape[1] == 1:
                        vals_g = [f"{float(x[i,0]):.3g}" for i in range(x.shape[0])]
                    elif hasattr(x, 'ndim') and x.ndim >= 2:
                        vals_g = ['[' + ','.join(f"{float(v):.3g}" for v in x[i].flatten()) + ']' for i in range(x.shape[0])]
                    else:
                        vals_g = []
                else:
                    vals_g = []
            except Exception:
                vals_g = []
            for v in vals_g:
                all_vals.append(v)
                try:
                    numeric_list.append(float(v))
                except Exception:
                    all_numeric = False

        palette_local = [mpl.colors.to_hex(c) for c in plt.get_cmap('tab20').colors]
        if all_vals:
            if all_numeric:
                nbins = min(len(palette_local), 8)
                if numeric_list:
                    vmin = min(numeric_list); vmax = max(numeric_list)
                else:
                    vmin = 0.0; vmax = 0.0
                if vmin == vmax:
                    bins = [vmin - 0.5, vmin + 0.5]
                else:
                    bins = list(np.linspace(vmin, vmax, nbins + 1))
                global_color_mapping = ('numeric', bins, palette_local)
            else:
                unique_all = sorted(list(dict.fromkeys(all_vals)))
                mapping = {val: palette_local[i % len(palette_local)] for i, val in enumerate(unique_all)}
                global_color_mapping = ('categorical', mapping, palette_local)

    if one_fig_per_step:
        # produce one file (or show) per step using the internal drawer so we can apply highlights
        for step in range(n_steps):
            data = graphs[step]

            op = edit_ops[step] if step < len(edit_ops) else None
            title = None
            if step > 0:
                title = f"Step {step}" + f": {edit_ops[step-1]}"

            fig, ax = plt.subplots(figsize=(8, 6))
            legend_info = _draw_graph_on_ax(data, ax, title=title, show_node_labels=show_labels, op=op, show_node_ids_outside=True, color_nodes_by_label=color_nodes_by_label, color_mapping=global_color_mapping)

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
                # add legend if needed for this single figure
                if legend_info is not None:
                    kind, payload = legend_info
                    fig.subplots_adjust(right=0.78)
                    if kind == 'colorbar':
                        fig.colorbar(payload, ax=ax, fraction=0.046, pad=0.04)
                    elif kind == 'categorical':
                        import matplotlib.patches as mpatches
                        patches = [mpatches.Patch(color=mpl.colors.to_hex(col), label=str(lbl)) for lbl, col in payload]
                        ax.legend(handles=patches, bbox_to_anchor=(1.02, 1), loc='upper left', borderaxespad=0.)
                fig.savefig(single_output, dpi=300)
                print(f"Saved edit path step to: {single_output}")
                plt.close(fig)
            else:
                plt.show()
    else:
        # single figure with subplots for each step: arrange into rows with max 5 columns
        max_cols = 5
        cols = min(max_cols, n_steps)
        rows = int(ceil(n_steps / cols))
        # figure size scales with number of columns and rows
        fig, axes = plt.subplots(rows, cols, figsize=(4 * cols, 3 * rows))
        # flatten axes regardless of shape
        import numpy as _np
        if rows == 1 and cols == 1:
            ax_list = [axes]
        else:
            try:
                ax_list = list(_np.array(axes).flatten())
            except Exception:
                # final fallback: try to iterate
                try:
                    ax_list = [a for a in axes]
                except Exception:
                    ax_list = [axes]

        legend_info = None
        for i in range(rows * cols):
            if i < n_steps:
                ax = ax_list[i]
                data = graphs[i]
                op = edit_ops[i] if i < len(edit_ops) else None
                title = None if i < len(edit_ops) else f"Target Graph"
                li = _draw_graph_on_ax(data, ax, title=title, show_node_labels=show_labels, op=op, show_node_ids_outside=True, color_nodes_by_label=color_nodes_by_label, color_mapping=global_color_mapping)
                if legend_info is None and li is not None:
                    legend_info = li
            else:
                ax = ax_list[i]
                ax.set_visible(False)

        plt.tight_layout()
        # if we have a legend for colored nodes, place it on the figure's right
        if color_nodes_by_label and legend_info is not None:
            kind, payload = legend_info
            fig.subplots_adjust(right=0.78)
            if kind == 'colorbar':
                fig.colorbar(payload, ax=axes, fraction=0.046, pad=0.04)
            elif kind == 'categorical':
                import matplotlib.patches as mpatches
                patches = [mpatches.Patch(color=mpl.colors.to_hex(col), label=str(lbl)) for lbl, col in payload]
                fig.legend(handles=patches, bbox_to_anchor=(0.98, 0.5), loc='center left')

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
