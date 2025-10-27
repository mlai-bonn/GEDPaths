from __future__ import annotations
import io
import os
import struct
from dataclasses import dataclass
from typing import List, Optional

import numpy as np
import torch
from torch_geometric.data import Data, InMemoryDataset


# -------- stdlib helpers --------

def _read_exact(f: io.BufferedReader, n: int) -> bytes:
    b = f.read(n)
    if len(b) != n:
        raise EOFError("Unexpected EOF.")
    return b

def _read_int(f, endian: str) -> int:
    return struct.unpack(endian + "i", _read_exact(f, 4))[0]

def _read_uint(f, endian: str) -> int:
    return struct.unpack(endian + "I", _read_exact(f, 4))[0]

def _read_size_t(f, endian: str, size_t_bytes: int) -> int:
    fmt = "Q" if size_t_bytes == 8 else "I"
    return struct.unpack(endian + fmt, _read_exact(f, size_t_bytes))[0]

def _read_string(f, endian: str) -> str:
    n = _read_uint(f, endian)
    if n == 0:
        return ""
    return _read_exact(f, n).decode("utf-8", errors="strict")

def _read_np_block(f, dtype: np.dtype, count: int) -> np.ndarray:
    if count == 0:
        return np.empty((0,), dtype=dtype)
    buf = _read_exact(f, count * dtype.itemsize)
    arr = np.frombuffer(buf, dtype=dtype, count=count)
    if arr.size != count:
        raise EOFError("Short read.")
    return arr

def _read_torch_block(f, dtype: torch.dtype, count: int) -> torch.Tensor:
    if count == 0:
        return torch.empty((0,), dtype=dtype)
    buf = _read_exact(f, count * torch.tensor([], dtype=dtype).element_size())
    arr = torch.frombuffer(buf, dtype=dtype)
    if arr.numel() != count:
        raise EOFError("Short read.")
    return arr


# -------- two-pass layout support --------

@dataclass
class _GraphHeader:
    name: str
    graph_type: int
    node_number: int
    node_features: int
    node_feature_names: List[str]
    edge_number: int
    edge_features: int
    edge_feature_names: List[str]


def load_bgf_two_pass_to_pyg_data_list_numpy(
        path: str,
        *,
        endian: str = "<",          # '<' little-endian, '>' big-endian
        size_t_bytes: int = 8,      # 8 for 64-bit writers, 4 for 32-bit
        keep_feature_names: bool = True,
        out_dtype: np.dtype = np.float32,
) -> List[Data]:
    """
    Loader for a BGF layout where:
      - For all graphs: headers + edge lists are written first (pass 1),
      - Then, for each graph in order: node features followed by edge features (pass 2).
    Uses NumPy frombuffer on raw bytes for fast bulk reads.
    """
    assert endian in ("<", ">")
    assert size_t_bytes in (4, 8)
    st_dtype  = np.dtype("u8" if size_t_bytes == 8 else "u4").newbyteorder("<" if endian == "<" else ">")
    dbl_dtype = np.dtype("f8").newbyteorder("<" if endian == "<" else ">")

    headers: List[_GraphHeader] = []

    with open(path, "rb") as f:
        # ---- header ----
        compatibility_format_version = _read_int(f, endian)  # we store later on Data
        graph_number = _read_int(f, endian)
        if graph_number < 0 or graph_number > 10**7:
            raise ValueError("Unreasonable graph count; check endianness/size_t.")

        # ---- PASS 1: read per-graph metadata + topology (edges) for ALL graphs ----
        for _ in range(graph_number):
            name = _read_string(f, endian)
            gtype = _read_int(f, endian)

            n  = _read_size_t(f, endian, size_t_bytes)
            nf = _read_uint(f, endian)
            node_feature_names = [_read_string(f, endian) for _ in range(nf)]

            m  = _read_size_t(f, endian, size_t_bytes)
            ef = _read_uint(f, endian)
            edge_feature_names = [_read_string(f, endian) for _ in range(ef)]

            headers.append(_GraphHeader(
                name=name,
                graph_type=int(gtype),
                node_number=int(n),
                node_features=int(nf),
                node_feature_names=node_feature_names,
                edge_number=int(m),
                edge_features=int(ef),
                edge_feature_names=edge_feature_names,
            ))

        # ---- PASS 2: for each graph in order, read node features then edge features ----
        data_list: List[Data] = []
        for h in headers:
            # Node feature matrix
            if h.node_features > 0:
                nf_arr = _read_torch_block(f, torch.float64, h.node_number * h.node_features).numpy()
                x = nf_arr.reshape((h.node_number, h.node_features))
            else:
                x = None

            # Read the edge indices
            for _i in range(h.edge_number):
                u = _read_size_t(f, endian, size_t_bytes)
                v = _read_size_t(f, endian, size_t_bytes)
                if u >= h.node_number or v >= h.node_number:
                    raise ValueError("Invalid edge index; check endianness/size_t.")
                # Store as 0-based indices
                if _i == 0:
                    edge_index = torch.empty((2, h.edge_number), dtype=torch.long)
                edge_index[0, _i] = u
                edge_index[1, _i] = v

                # Edge feature matrix
                if h.edge_features > 0 and h.edge_number > 0:
                    ef_arr = _read_torch_block(f, torch.float64, h.edge_features)
                    if _i == 0:
                        edge_attr = torch.empty((h.edge_number, h.edge_features), dtype=torch.float64)

                    edge_attr[_i, :] = ef_arr

                else:
                    edge_attr = None

            d = Data(
                x=x,
                edge_index=edge_index,
                edge_attr=edge_attr,
            )
            d.num_nodes = h.node_number
            d.bgf_name = h.name
            d.bgf_graph_type = h.graph_type
            d.bgf_version = int(compatibility_format_version)
            # split the name into start graph end graph and edit step
            d.bgf_name_parts = h.name.split("_")
            d.edit_path_start = d.bgf_name_parts[-3]
            d.edit_path_end = d.bgf_name_parts[-2]
            d.edit_path_step = d.bgf_name_parts[-1]
            if keep_feature_names:
                d.node_feature_names = h.node_feature_names
                d.edge_feature_names = h.edge_feature_names
            data_list.append(d)

    return data_list


# -------- optional: InMemoryDataset wrapper --------

class BGFInMemoryDataset(InMemoryDataset):
    """
    Wrapper around the two-pass loader.
    """
    def __init__(
            self,
            root: str,
            path: str,
            *,
            endian: str = "<",
            size_t_bytes: int = 8,
            keep_feature_names: bool = True,
            out_dtype: np.dtype = np.float32,
            transform=None,
            pre_transform=None,
            pre_filter=None,
    ):
        self._bgf_path = path
        self._endian = endian
        self._size_t_bytes = size_t_bytes
        self._keep_feature_names = keep_feature_names
        self._out_dtype = out_dtype
        super().__init__(root, transform, pre_transform, pre_filter)
        self.data, self.slices = torch.load(self.processed_paths[0], weights_only=False)

    @property
    def raw_file_names(self) -> List[str]:
        return [os.path.basename(self._bgf_path)]

    @property
    def processed_file_names(self) -> List[str]:
        return ["data.pt"]

    def download(self):
        if not os.path.isfile(self._bgf_path):
            raise FileNotFoundError(self._bgf_path)

    def process(self):
        data_list = load_bgf_two_pass_to_pyg_data_list_numpy(
            self._bgf_path,
            endian=self._endian,
            size_t_bytes=self._size_t_bytes,
            keep_feature_names=self._keep_feature_names,
            out_dtype=self._out_dtype,
        )
        if self.pre_filter is not None:
            data_list = [d for d in data_list if self.pre_filter(d)]
        if self.pre_transform is not None:
            data_list = [self.pre_transform(d) for d in data_list]
        data, slices = self.collate(data_list)
        os.makedirs(self.processed_dir, exist_ok=True)
        torch.save((data, slices), self.processed_paths[0])

