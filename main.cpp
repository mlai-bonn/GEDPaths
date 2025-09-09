// define gurobi
#define GUROBI
// use gedlib
#define GEDLIB

#include <filesystem>
#include <iostream>
#include <vector>
#include "GraphDataStructures/GraphBase.h"
#include "src/env/ged_env.hpp"
#include <LoadSave.h>
#include "create_edit_paths.h"
#include "load_tu.h"


int main() {
    auto databases = {"MUTAG", "NCI1"};
    const std::string input_path = "../Data/Graphs/";
    const std::string output_path = "../Data/ProcessedGraphs/";
    const std::string edit_path_output = "../Data/EditPaths/";
    const std::string mapping_path_output = "../Data/Mappings/";
    // create output directory if it does not exist
    if (!std::filesystem::exists(edit_path_output)) {
        std::filesystem::create_directory(edit_path_output);
    }
    // create mapping output directory
    if (!std::filesystem::exists(mapping_path_output)) {
        std::filesystem::create_directory(mapping_path_output);
    }


    if (bool success = create_tu("MUTAG", input_path, output_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<GraphStruct> graphs = load_tu("MUTAG", output_path);
    graphs.SetName("MUTAG");
    ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
    env.set_edit_costs(ged::Options::EditCosts::CONSTANT);
    initialize_env(env, graphs);
    env.set_method(ged::Options::GEDMethod::REFINE);
    env.init_method();
    EvaluateGEDResults(env, graphs, mapping_path_output);
    // load mappings
    std::vector<GEDEvaluation> results;
    BinaryToGEDResult(mapping_path_output + "MUTAG_ged_mapping.bin", graphs, results);
    CreateAllEditPaths(results, graphs,  edit_path_output);
    // Load MUTAG edit paths
    GraphData<GraphStruct> edit_paths;
    edit_paths.Load(edit_path_output + "MUTAG_edit_paths.bgf");
    return 0;
}