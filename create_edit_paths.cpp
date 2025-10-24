#include <filesystem>
#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include "GraphDataStructures/GraphBase.h"
#include "LoadSaveGraphDatasets.h"
#include "LoadSave.h"
#include "Algorithms/GED/GEDFunctions.h"

std::vector<int> CheckResultsValidity(const std::vector<GEDEvaluation<UDataGraph>>& results) {
    std::vector<int> invalids;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        const auto& fst = result.node_mapping.first;
        const auto& snd = result.node_mapping.second;
        auto first_set = std::set<std::decay_t<decltype(fst[0])>>{};
        for (const auto& v : fst) first_set.insert(v);
        auto second_set = std::set<std::decay_t<decltype(snd[0])>>{};
        for (const auto& v : snd) second_set.insert(v);
        bool has_duplicate = (first_set.size() != fst.size() && second_set.size() != snd.size());
        bool distance_not_integer = (std::abs(result.distance - std::round(result.distance)) > 1e-6);
        if (has_duplicate || distance_not_integer) {
            invalids.push_back(static_cast<int>(i));
        }
    }
    return invalids;
}

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for loading the mappings
    std::string mappings_path = "../Results/Mappings/";
    // -num_mappings argument for the number of valid mappings to create edit paths for
    int num_mappings = 1000;
    // -seed argument for the random seed
    int seed = 42;
    // -edit_paths argument for the path to store the edit paths
    std::string edit_path_output = "../Results/Paths/";
    // -t arguments for the threads to use
    int num_threads = 1;
    // -method
    auto method = "REFINE";

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
        else if (std::string(argv[i]) == "-num_mappings") {
            num_mappings = std::stoi(argv[i+1]);
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
    // add method and db to the output path
    mappings_path = mappings_path  + method + "/" + db + "/";
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
    // Check validity and collect invalid result ids
    auto invalids = CheckResultsValidity(results);
    if (!invalids.empty()) {
        std::cerr << "Warning: Found invalid mappings for the following result ids (these will be skipped):\n";
        for (const auto &id : invalids) {
            std::cerr << "  " << id << "\n";
        }
    }
    // Filter out invalid results
    std::vector<GEDEvaluation<UDataGraph>> valid_results;
    for (size_t i = 0; i < results.size(); ++i) {
        if (std::find(invalids.begin(), invalids.end(), static_cast<int>(i)) == invalids.end()) {
            valid_results.push_back(results[i]);
        }
    }
    if (valid_results.empty()) {
        std::cerr << "No valid results to process. Exiting.\n";
        return 1;
    }
    if (num_mappings > 0 && num_mappings < static_cast<int>(valid_results.size())) {
        // shuffle valid_results and take first num_mappings
        std::ranges::shuffle(valid_results, std::mt19937(seed));
        valid_results.resize(num_mappings);
        // sort valid_results by graph ids
        std::ranges::sort(valid_results, [](const GEDEvaluation<UDataGraph>& a, const GEDEvaluation<UDataGraph>& b) {
            if (a.graph_ids.first != b.graph_ids.first) {
                return a.graph_ids.first < b.graph_ids.first;
            }
            return a.graph_ids.second < b.graph_ids.second;
        });

    }
    // print info about number of valid results considered
    std::cout << "Creating edit paths for " << valid_results.size() << " valid mappings out of " << results.size() << " total mappings.\n";
    CreateAllEditPaths(valid_results, graphs,  edit_path_output_db);

    return 0;
}
