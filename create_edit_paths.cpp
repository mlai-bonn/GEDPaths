#include <filesystem>
#include <iostream>
#include <vector>
#include "GraphDataStructures/GraphBase.h"
#include <LoadSave.h>
#include "LoadSaveGraphDatasets.h"
#include "Algorithms/GED/GEDLIBWrapper.h"
#include "Algorithms/GED/GEDFunctions.h"
bool CheckResultsValidity(const std::vector<GEDEvaluation<UDataGraph>>& results) {

    for (auto& result : results) {
        const auto&[fst, snd] = result.node_mapping;
        // check whether one or the other has no duplicates
        std::set<NodeId> first_set = {fst.begin(), fst.end()};
        std::set<NodeId> second_set = {snd.begin(), snd.end()};
        if (first_set.size() != fst.size() && second_set.size() != snd.size()) {
            std::cerr << "Error: Mapping has duplicates for graphs " << result.graph_ids.first << " and " << result.graph_ids.second << std::endl;
            return false;
        }
    }
    return true;
}

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for loading the mappings
    std::string mappings_path = "../Data/Results/Mappings/REFINE/MUTAG/";
    // -method
    std::string method = "REFINE";
    // -edit_paths argument for the path to store the edit paths
    std::string edit_path_output = "../Data/Results/Paths/";
    // -t arguments for the threads to use
    int num_threads = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-mappings") {
            mappings_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-t") {
            num_threads = std::stoi(argv[i+1]);
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            // TODO
             return 0;
        }
        else {
            std::cout << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }

    }
    std::string edit_path_output_db = edit_path_output + method + "/" + db + "/";
    if (!std::filesystem::exists(edit_path_output)) {
        std::filesystem::create_directory(edit_path_output);
    }
    if (!std::filesystem::exists(edit_path_output_db)) {
        std::filesystem::create_directories(edit_path_output_db);
    }

    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData(db, processed_graph_path, graphs);


    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(mappings_path + db + "_ged_mapping.bin", graphs, results);
    CheckResultsValidity(results);
    CreateAllEditPaths(results, graphs,  edit_path_output_db);

    return 0;
}

