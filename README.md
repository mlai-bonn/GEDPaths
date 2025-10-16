# GEDPaths
Repo that builds up on the libgraph and uses the GEDLIB to create edit paths between two pairs of graphs from different sources.

## Installation
[Instruction](INSTALLATION.md) to install all dependencies and the repo.

## Usage
### Compute mappings 
1. Download a Dataset from [TUDortmund](https://chrsmrrs.github.io/datasets/) or use your own graphs in the same format into the [Data](Data) folder.
2. Build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   make -j 6
   ```
3. Run the mapping computation:
   ```bash
   ./create_edit_mappings \
     -db MUTAG \
     -raw ../Data/Graphs/ \
     -processed ../Data/ProcessedGraphs/ \
     -mappings ../Results/Mappings/ \
     -t 4 \
     -method REFINE \
     -cost CONSTANT \
     -seed 42 \
     -num_graphs 1000
   ```
   - Main arguments:
     - `-db <database name>`: Name of the dataset (e.g., MUTAG)
     - `-raw <raw data path>`: Path to raw graph data
     - `-processed <processed data path>`: Path to store processed graphs
     - `-mappings <output path>`: Path to store mappings
     - `-t <threads>`: Number of threads
     - `-method <method>`: GED method (e.g., REFINE)
     - `-cost <cost>`: Edit cost type (e.g., CONSTANT)
     - `-seed <seed>`: Random seed
     - `-num_graphs <N>`: Number of graph pairs (optional)

### Compute edit paths
1. Build the project (if not already built):
   ```bash
   mkdir build
   cd build
   cmake ..
   make -j 6
   ```
2. Run the edit path computation:
   ```bash
   ./create_edit_paths \
     -db MUTAG \
     -processed ../Data/ProcessedGraphs/ \
     -mappings ../Results/Mappings/REFINE/MUTAG/ \
     -t 1
   ```
   - Main arguments:
     - `-db <database name>`: Name of the dataset
     - `-processed <processed data path>`: Path to processed graphs
     - `-mappings <mappings path>`: Path to mappings
     - `-t <threads>`: Number of threads

### Export to Pytorch Geometric Format
