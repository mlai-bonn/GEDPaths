
## Examples of creating edit mappings for different methods
```bash
./create_edit_mappings  \
-db MUTAG -method F2 -method_options threads 30
```

```bash
./create_edit_mappings \
-db MUTAG -method REFINE -method_options threads 30 max-swap-size 5 lower-bound-method BRANCH
```

## Examples of creating paths with different strategies
```bash
./create_edit_paths \
-method F2 -path_strategy Random
```

```bash
./create_edit_paths \
-method REFINE -path_strategy Random DeleteIsolateNodes
```

```bash
./create_edit_paths \
-method REFINE -path_strategy InsertEdges DeleteIsolateNodes
```

```bash
./create_edit_paths \
-method REFINE -path_strategy DeleteEdges DeleteIsolateNodes
```

