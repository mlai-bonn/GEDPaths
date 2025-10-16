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
    // -db argument for the database
    std::string db = "MUTAG";
    // -raw argument for the raw data path
    std::string input_path = "../Data/Graphs/";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for the path to store the mappings
    std::string output_path = "../Results/Mappings/";
    // -t arguments for the threads to use
    int num_threads = 4;
    // -method
    auto method = "REFINE";
    auto ged_method = GEDMethodFromString(method);
    // -cost
    auto cost = "CONSTANT";
    auto edit_cost = EditCostsFromString(cost);
    // -s
    auto seed = 42;
    // -ids_path
    std::string graph_ids_path;
    // -num_pairs to randomly sample from the dataset and create mappings for (-1 for all)
    int num_pairs = 1000;
    std::vector<std::pair<INDEX, INDEX>> graph_ids;


    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-raw") {
            input_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-mappings") {
            output_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-t") {
            num_threads = std::stoi(argv[i+1]);
        }
        else if (std::string(argv[i]) == "-method") {
            method = argv[i+1];
            ged_method = GEDMethodFromString(method);
        }
        else if (std::string(argv[i]) == "-cost") {
            cost = argv[i+1];
            edit_cost = EditCostsFromString(cost);
        }
        else if (std::string(argv[i]) == "-seed") {
            seed = std::stoi(argv[i+1]);
        }
        else if (std::string(argv[i]) == "-ids_path") {
            graph_ids_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-num_graphs") {
            num_pairs = std::stoi(argv[i+1]);
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            std::cout << "Create edit mappings for a given database" << std::endl;
            std::cout << "Arguments:" << std::endl;
            std::cout << "-db <database name>" << std::endl;
            std::cout << "-raw <raw data path where db can be found>" << std::endl;
            std::cout << "-processed <processed data path>" << std::endl;
            std::cout << "-mappings <mappings path>" << std::endl;
            std::cout << "-help <show this help message>" << std::endl;
            std::cout << "Usage: " << argv[0] << " -db <database name> -raw <raw data path where db can be found> -processed <processed data path> -mappings <mappings path>" << std::endl;
            return 0;
        }
        else if (std::string(argv[i]) == "-") {
            // do nothing for lone -
        }
        // if -something else then print error
        else if (std::string(argv[i]).rfind('-', 0) == 0) {
            std::cout << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }

    }
    // set up random device
    auto gen = std::mt19937(seed);

    // create mapping output directory
    if (!std::filesystem::exists(output_path)) {
        std::filesystem::create_directory(output_path);
    }
    // create folder for method under output path
    output_path = output_path + method + "/";
    std::filesystem::create_directory(output_path + "/");
    std::filesystem::create_directory(output_path + "/" + db + "/");
    std::filesystem::create_directory(output_path + "/" + db + "/tmp/");

    if (bool success = LoadSaveGraphDatasets::PreprocessTUDortmundGraphData(db, input_path, processed_graph_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData(db, processed_graph_path, graphs);

    if (!graph_ids_path.empty()) {
        // load graph ids from file
        std::ifstream id_file(graph_ids_path);
        if (!id_file.is_open()) {
            std::cerr << "Could not open the graph ids file: " << graph_ids_path << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(id_file, line)) {
            std::istringstream iss(line);
            int id1, id2;
            if (!(iss >> id1 >> id2)) {
                std::cerr << "Error reading graph ids from line: " << line << std::endl;
                continue;
            }
            if (id1 < 0 || id1 >= graphs.graphData.size() || id2 < 0 || id2 >= graphs.graphData.size()) {
                std::cerr << "Graph ids out of range: " << id1 << ", " << id2 << std::endl;
                continue;
            }
            graph_ids.emplace_back(id1, id2);
        }
    }
    else {
        if (num_pairs > 0 && num_pairs <= graphs.graphData.size() * (graphs.graphData.size() - 1) / 2) {
            while (graph_ids.size() < num_pairs) {
                INDEX id1 = gen() % graphs.graphData.size();
                // id2 should between id1 + 1 and graphs.graphData.size() - 1
                INDEX id2 = gen() % graphs.graphData.size();
                if (id1 != id2) {
                    std::pair<INDEX, INDEX> pair = std::minmax(id1, id2);
                    if (ranges::find(graph_ids, pair) == graph_ids.end()) {
                        graph_ids.emplace_back(pair);
                    }
                }
            }
            // sort graph ids by first and then second
            ranges::sort(graph_ids, [](const std::pair<INDEX, INDEX>& a, const std::pair<INDEX, INDEX>& b) {
                return a.first == b.first ? a.second < b.second : a.first < b.first;
            });

            // write to id file
            std::string out_file = output_path + db + "/" + "graph_ids.txt";
            std::ofstream id_file(out_file);
            for (const auto& ids : graph_ids) {
                id_file << ids.first << " " << ids.second << std::endl;
            }
            id_file.close();

        }
        else {
            for (auto i = 0; i < graphs.graphData.size(); ++i) {
                for ( auto j = i + 1; j < graphs.graphData.size(); ++j) {
                    graph_ids.emplace_back(i,j);
                }
            }
        }
    }

    // set omp number of threads to max threads of this machine
    const int chunk_size = std::min(100, static_cast<int>(graph_ids.size())/(2*num_threads));

    // shuffle the graph ids
    ranges::shuffle(graph_ids, gen);
    std::vector<std::vector<std::pair<INDEX, INDEX>>> graph_id_chunks;
    // split graph ids into chunks of size chunk_size
    for (int i = 0; i < graph_ids.size(); i += chunk_size) {
        graph_id_chunks.emplace_back(graph_ids.begin() + i, graph_ids.begin() + min(i + chunk_size, static_cast<int>(graph_ids.size())));
    }


    // parallelize computation of ged using openmp
#pragma omp parallel for schedule(dynamic) shared(graphs, graph_id_chunks, output_path, edit_cost, ged_method, db) default(none) num_threads(num_threads)
    for (const auto & graph_id_chunk : graph_id_chunks) {
        ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
        InitializeGEDEnvironment(env, graphs, edit_cost, ged_method);
        ComputeGEDResults(env, graphs, graph_id_chunk, output_path + db + "/tmp/");
    }
    std::string search_string = "_ged_mapping";
    MergeGEDResults(output_path + db + "/tmp/", output_path + db + "/", search_string, graphs);
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(output_path + db + "/" + db + "_ged_mapping.bin", graphs, results);
    CSVFromGEDResults(output_path + db + "/" + db + "_ged_mapping.csv", results);
    return 0;
}