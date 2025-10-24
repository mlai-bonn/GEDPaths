#include <filesystem>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include "GraphDataStructures/GraphBase.h"
#include "LoadSaveGraphDatasets.h"
#include "Algorithms/GED/GEDFunctions.h"

// Return list of invalid result indices (indices into the results vector)
std::vector<int> CheckResultsValidity(const std::vector<GEDEvaluation<UDataGraph>>& results) {
    std::vector<int> invalid_result_ids;

    for (size_t idx = 0; idx < results.size(); ++idx) {
        const auto& result = results[idx];
        const auto& [fst, snd] = result.node_mapping;
        // check whether one or the other has no duplicates
        std::set<NodeId> first_set = {fst.begin(), fst.end()};
        std::set<NodeId> second_set = {snd.begin(), snd.end()};
        if (first_set.size() != fst.size() && second_set.size() != snd.size()) {
            invalid_result_ids.push_back(static_cast<int>(idx));
        }
    }
    return invalid_result_ids;
}

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for loading the mappings
    std::string mappings_path = "../Results/Mappings/";
    // -edit_paths argument for the path to store the edit paths
    std::string edit_path_output = "../Results/Paths/";
    // -method
    std::string method = "REFINE";

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-mappings") {
            mappings_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-method") {
            method = argv[i+1];
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
    mappings_path += method + '/' + db + '/';
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
    auto invalid_ids = CheckResultsValidity(results);
    if (!invalid_ids.empty()) {
        std::cerr << "Found " << invalid_ids.size() << " invalid mapping result(s):\n";
        for (const auto& rid : invalid_ids) {
            const auto& res = results[rid];
            int id1 = static_cast<int>(res.graph_ids.first);
            int id2 = static_cast<int>(res.graph_ids.second);
            std::cerr << "  - Result index " << rid << " (graphs " << id1 << " and " << id2 << ") has duplicate mappings - removing\n";
        }
        // Efficiently filter out invalid results by index
        std::unordered_set<int> invalid_set(invalid_ids.begin(), invalid_ids.end());
        std::vector<GEDEvaluation<UDataGraph>> filtered_results;
        filtered_results.reserve(results.size() > invalid_set.size() ? results.size() - invalid_set.size() : 0);
        for (size_t i = 0; i < results.size(); ++i) {
            if (invalid_set.find(static_cast<int>(i)) == invalid_set.end()) {
                filtered_results.push_back(std::move(results[i]));
            }
        }
        results.swap(filtered_results);
    }

    CreateAllEditPaths(results, graphs,  edit_path_output_db);

    return 0;
}
