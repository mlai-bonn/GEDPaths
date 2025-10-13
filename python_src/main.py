from torch_geometric_exporter import BGFInMemoryDataset

ds = BGFInMemoryDataset(
    root="Data/Test/",  # any writable directory
    path="Data/Test/MUTAG_edit_paths.bgf",  # or .bgf
    endian="<",
    size_t_bytes=8,      # switch to 4 if the writer was 32-bit
)

print(len(ds))
g0 = ds[0]
print(g0)
print(g0.x.shape if g0.x is not None else None)
print(g0.edge_index.shape, g0.edge_attr.shape if g0.edge_attr is not None else None)
print(g0.bgf_name, getattr(g0, "node_feature_names", None), getattr(g0, "edge_feature_names", None), getattr(g0, "edit_path_start", None), getattr(g0, "edit_path_end", None), getattr(g0, "edit_path_step", None))