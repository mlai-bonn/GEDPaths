#include <filesystem>
#include <iostream>
#include <vector>

#include "SimplePatterns.h"
#include "Algorithms/GED/GEDFunctions.h"
#include "GraphDataStructures/GraphBase.h"
#include "src/env/ged_env.hpp"
#include <LoadSave.h>
#include <GraphFunctions.h>

#include "load_tu.h"


int main() {
    auto databases = {"MUTAG", "NCI1"};
    const std::string input_path = "../Data/";
    const std::string output_path = "../Data/ProcessedGraphs/";
    load_tu("MUTAG", input_path, output_path);
}
