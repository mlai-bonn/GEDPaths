#!/bin/bash
# This script is an end to end example of running all steps of an experiment:
# If not already done, create a virtual environment and install the required packages
# If .venv/bin/activate exists, activate it
if [ ! -f ".venv/bin/activate" ]; then
  # ask the user if they want to create a virtual environment (y/n)
  read -r -p "Virtual environment not found. Do you want to create one? (y/n) " response
  if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    python3 -m venv .venv
    source v.env/bin/activate
    pip install --upgrade pip
    pip install -r python_src/requirements.txt
  else
    echo "Please create a virtual environment and install the required packages from python_src/requirements.txt"
    exit 1
  fi
else
  source .venv/bin/activate
fi

# use the dataset as input parameter (default: MUTAG)
dataset="${1:-MUTAG}"
echo "Using dataset: $dataset"


# load the data (Mutagenicity dataset)
python python_src/data_loader.py -db "$dataset"
#compile the C++ code
mkdir build
cd build || exit
#cmake .. && make
# create the mappings for 5000 random pairs of graphs
./CreateMappings -db "$dataset" -num_pairs 5000 -method F2 -method_options threads 30
# create the paths with different strategies
#./CreatePaths -db "$dataset" -method F2 -path_strategy Random
./CreatePaths -db "$dataset" -method F2 -path_strategy Random DeleteIsolatedNodes
#./CreatePaths -db "$dataset" -method F2 -path_strategy InsertEdges DeleteIsolatedNodes
#./CreatePaths -db "$dataset" -method F2 -path_strategy DeleteEdges DeleteIsolatedNodes
# convert the generated path graphs to pytorch-geometric format
#python ../python_src/converter/bgf_to_pt.py -db "$dataset" -method F2 -path_strategy Rnd
python ../python_src/converter/bgf_to_pt.py -db "$dataset" -method F2 -path_strategy Rnd_d-IsoN
#python ../python_src/converter/bgf_to_pt.py -db "$dataset" -method F2 -path_strategy i-E_d-IsoN
#python ../python_src/converter/bgf_to_pt.py -db "$dataset" -method F2 -path_strategy d-E_d-IsoN


