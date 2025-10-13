#include <filesystem>
#include <iostream>
#include <vector>
#include "GraphDataStructures/GraphBase.h"
#include <LoadSave.h>
#include "LoadSaveGraphDatasets.h"
#include "Algorithms/GED/GEDLIBWrapper.h"
#include "Algorithms/GED/GEDFunctions.h"

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string output_path = "../Data/ProcessedGraphs/";
    // -mappings argument for loading the mappings
    std::string mappings_path = "../Data/Results/";
    // -method
    std::string method = "F1";
    // -edit_paths argument for the path to store the edit paths
    std::string edit_path_output = "../Data/Results/Paths/";
    // -t arguments for the threads to use
    int num_threads = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-processed") {
            output_path = argv[i+1];
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
    mappings_path = mappings_path + "/" + method + "/" + db + "/";

    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData(db, output_path, graphs);


    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(mappings_path + "MUTAG_ged_mapping.bin", graphs, results);
    CreateAllEditPaths(results, graphs,  edit_path_output);
    // Load MUTAG edit paths
    GraphData<UDataGraph> edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>> edit_path_info;
    edit_paths.Load(edit_path_output + "MUTAG_edit_paths.bgf");
    std::string info_path = edit_path_output + "MUTAG_edit_paths_info.bin";
    ReadEditPathInfo(info_path, edit_path_info);
    return 0;
}