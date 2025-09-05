// define gurobi

#define GUROBI

#include <filesystem>
#include <iostream>
#include <vector>

#include "SimplePatterns.h"
#include "Algorithms/GED/GEDFunctions.h"
#include "GraphDataStructures/GraphBase.h"
#include "src/env/ged_env.hpp"
#include <LoadSave.h>
#include <GraphFunctions.h>

#include "create_edit_paths.h"
#include "load_tu.h"


int main() {
    auto databases = {"MUTAG", "NCI1"};
    const std::string input_path = "../Data/";
    const std::string output_path = "../Data/ProcessedGraphs/";
    create_tu("MUTAG", input_path, output_path);
    GraphData<GraphStruct> graphs = load_tu("MUTAG", output_path);
    ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
    env.set_edit_costs(ged::Options::EditCosts::CONSTANT);
    initialize_env(env, graphs);
    env.set_method(ged::Options::GEDMethod::F1);
    env.init_method();

}
