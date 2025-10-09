# GEDPaths
Repo that builds up on the libgraph and uses the GEDLIB to create edit paths between two pairs of graphs from different sources.

## Installation
[Instruction](INSTALLATION.md) to install all dependencies and the repo.

## Usage
### Compute mappings 
1. Download a Dataset from [TUDortmund](https://chrsmrrs.github.io/datasets/) or use your own graphs in the same format into the [Data](Data) folder.
2. Run the create_edit_mappings.cpp using:
  ```bash
    mkdir build
    cd build
    cmake ..
    make -j 6
    ./create_edit_mappings -i ../Data/ -o ../Data/Mappings/ 
  ```

### Compute edit paths
Run the [create_edit_paths.cpp](src/create_edit_paths.cpp) using:
  ```bash
    mkdir build
    cd build
    cmake ..
    make -j 6
    ./create_edit_paths -i ../Data/Mappings/ -o ../Data/Paths/ 
  ```
### Export to Pytorch Geometric Format

