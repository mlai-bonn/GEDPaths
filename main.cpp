// define gurobi
#define GUROBI
// use gedlib
#define GEDLIB

#include <filesystem>
#include <iostream>
#include <vector>
#include "GraphDataStructures/GraphBase.h"
#include "src/env/ged_env.hpp"
#include <LoadSaveGraphDatasets.h>

#include "Algorithms/GED/GEDLIBWrapper.h"
#include "Algorithms/GED/GEDFunctions.h"


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


    if (bool success = LoadSaveGraphDatasets::PreprocessTUDortmundGraphData("MUTAG", input_path, output_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData("MUTAG", output_path, graphs);
    // set omp number of threads to max threads of this machine
    int num_threads_used = 30;
    int x = omp_get_num_threads();
    const int chunk_size = std::min(100, static_cast<int>(graphs.graphData.size() * graphs.graphData.size() - 1)/(2*num_threads_used));
    std::vector<std::pair<INDEX, INDEX>> graph_ids;
    for (auto i = 0; i < graphs.graphData.size(); ++i) {
        for ( auto j = i + 1; j < graphs.graphData.size(); ++j) {
            graph_ids.emplace_back(i,j);
        }
    }
    // shuffle the graph ids
    ranges::shuffle(graph_ids, std::mt19937{std::random_device{}()});
    std::vector<std::vector<std::pair<INDEX, INDEX>>> graph_id_chunks;
    // split graph ids into chunks of size chunk_size
    for (int i = 0; i < graph_ids.size(); i += chunk_size) {
        graph_id_chunks.emplace_back(graph_ids.begin() + i, graph_ids.begin() + min(i + chunk_size, (int)graph_ids.size()));
    }


    // parallelize computation of ged using openmp
#pragma omp parallel for schedule(dynamic) shared(graphs, graph_id_chunks, mapping_path_output) default(none) num_threads(num_threads_used)
    for (const auto & graph_id_chunk : graph_id_chunks) {
        ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
        InitializeGEDEnvironment(env, graphs, ged::Options::EditCosts::CONSTANT, ged::Options::GEDMethod::F1);
        ComputeGEDResults(env, graphs, graph_id_chunk, mapping_path_output);
    }
    std::string search_string = "_ged_mapping";
    MergeGEDResults(mapping_path_output, search_string, graphs);
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(mapping_path_output + "MUTAG_ged_mapping.bin", graphs, results);
    CSVFromGEDResults(mapping_path_output + "MUTAG_ged_mapping.csv", results);
    CreateAllEditPaths(results, graphs,  edit_path_output);
    // Load MUTAG edit paths
    GraphData<UDataGraph> edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX>> edit_path_info;
    edit_paths.Load(edit_path_output + "MUTAG_edit_paths.bgf");
    std::string info_path = edit_path_output + "MUTAG_edit_paths_info.bin";
    ReadEditPathInfo(info_path, edit_path_info);
    return 0;
}