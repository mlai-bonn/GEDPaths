
#include <filesystem>
#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <random>
#include <algorithm>
#include <libGraph.h>


// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for loading the mappings
    std::string mappings_path = "../Results/Mappings/";
    // -num_mappings argument for the number of valid mappings to create edit paths for
    int num_mappings = -1;
    // -seed argument for the random seed
    int seed = 42;
    // -edit_paths argument for the path to store the edit paths
    std::string edit_path_output = "../Results/Paths/";
    // -t arguments for the threads to use
    int num_threads = 1;
    // -method
    std::string method = "REFINE";
    std::string path_strategy = "Random";
    EditPathStrategy edit_path_strategy = EditPathStrategy::Random;
    bool connected_only = false;

    int source_id = -1;
    int target_id = -1;
    std::string method_options;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-db" || std::string(argv[i]) == "-data" || std::string(argv[i]) == "-dataset" || std::string(argv[i]) == "-database") {
            db = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-mappings") {
            mappings_path = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-method") {
            method = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-num_mappings") {
            num_mappings = std::stoi(argv[i+1]);
            ++i;
        }
        else if (std::string(argv[i]) == "-source_id") {
            source_id = std::stoi(argv[i+1]);
            ++i;
        }
        else if (std::string(argv[i]) == "-target_id") {
            target_id = std::stoi(argv[i+1]);
            ++i;
        }
        else if (std::string(argv[i]) == "-path_strategy") {
            path_strategy = argv[i+1];
            edit_path_strategy = EditPathStrategyFromString(path_strategy);
            ++i;
        }
        else if (std::string(argv[i]) == "-connected_only") {
            connected_only = true;
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            std::cout << "Create edit paths from GED mappings" << std::endl;
            std::cout << "Arguments:" << std::endl;
            std::cout << "-db | -data | -dataset | -database <database name>" << std::endl;
            std::cout << "-processed <processed data path>" << std::endl;
            std::cout << "-mappings <mappings path>" << std::endl;
            std::cout << "-num_mappings <number of mappings to consider>" << std::endl;
            std::cout << "-method <GED method name>" << std::endl;
            std::cout << "-source_id <source graph id>" << std::endl;
            std::cout << "-target_id <target graph id>" << std::endl;
            std::cout << "-help <show this help message>" << std::endl;
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
            std::cerr << "  " << id << ": " << "Graph IDs (" << results[id].graph_ids.first << ", " << results[id].graph_ids.second << ")\n";
        }
    }
    else {
        std::cout << "All loaded mappings are valid.\n";
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
    std::cout << "Proceeding with " << valid_results.size() << " valid mappings out of " << results.size() << " total mappings.\n";

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
    // Falls source_id und target_id gesetzt sind, nur das entsprechende Mapping verwenden
    if (source_id >= 0 && target_id >= 0) {
        std::cout << "Creating edit path for specific graph IDs: " << source_id << " and " << target_id << ".\n";
        auto it = std::find_if(results.begin(), results.end(), [&](const GEDEvaluation<UDataGraph>& eval) {
            return (eval.graph_ids.first == source_id && eval.graph_ids.second == target_id) ||
                   (eval.graph_ids.first == target_id && eval.graph_ids.second == source_id);
        });
        if (it == results.end()) {
            std::cerr << "Kein Mapping für die angegebenen Graphen-IDs gefunden.\n";
            return 1;
        }
        std::vector<GEDEvaluation<UDataGraph>> single_result{*it};
        std::cout << "Erzeuge Edit-Path nur für Mapping zwischen Graph " << source_id << " und " << target_id << ".\n";
        CreateAllEditPaths(single_result, graphs,  edit_path_output_db, connected_only, path_strategy);
        return 0;
    }
    // print info about number of valid results considered
    std::cout << "Creating edit paths for " << valid_results.size() << " valid mappings out of " << results.size() << " total mappings.\n";
    CreateAllEditPaths(valid_results, graphs,  edit_path_output_db, connected_only, path_strategy);

    return 0;
}
