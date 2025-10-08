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
#include "LoadSaveGraphDatasets.h"
#include "Algorithms/GED/GEDLIBWrapper.h"
#include "Algorithms/GED/GEDFunctions.h"

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    auto databases = {"MUTAG", "NCI1"};
    const std::string input_path = "../Data/Graphs/";
    const std::string output_path = "../Data/ProcessedGraphs/";
    const std::string edit_path_output = "../Data/Test/";
    const std::string mapping_path_output = "../Data/Test/";
    // create output directory if it does not exist
    if (!std::filesystem::exists(edit_path_output)) {
        std::filesystem::create_directory(edit_path_output);
    }
    // create mapping output directory
    if (!std::filesystem::exists(mapping_path_output)) {
        std::filesystem::create_directory(mapping_path_output);
    }


    if (const bool success = LoadSaveGraphDatasets::PreprocessTUDortmundGraphData("MUTAG", input_path, output_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData("MUTAG", output_path, graphs);

    // For test take only graphs 0,1
    int source_id = 0;
    int target_id = 1;
    if (argc == 3) {
        source_id = std::stoi(argv[1]);
        target_id = std::stoi(argv[2]);
    }
    std::vector<std::pair<INDEX, INDEX>> graph_id_chunk = {{source_id,target_id}};

    ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
    InitializeGEDEnvironment(env, graphs, ged::Options::EditCosts::CONSTANT, ged::Options::GEDMethod::F2);
    ComputeGEDResults(env, graphs, graph_id_chunk, mapping_path_output);

    std::string search_string = "_ged_mapping";
    MergeGEDResults(mapping_path_output, search_string, graphs);
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(mapping_path_output + "MUTAG_ged_mapping.bin", graphs, results);
    CSVFromGEDResults(mapping_path_output + "MUTAG_ged_mapping.csv", results);
    CreateAllEditPaths(results, graphs,  edit_path_output, true);
    // Load MUTAG edit paths
    GraphData<UDataGraph> edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX>> edit_path_info;
    edit_paths.Load(edit_path_output + "MUTAG_edit_paths.bgf");
    std::string info_path = edit_path_output + "MUTAG_edit_paths_info.bin";
    ReadEditPathInfo(info_path, edit_path_info);
    // delete all files in the test folder
    //std::filesystem::remove_all(edit_path_output);
    return 0;
}