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
    std::string output_path = "../Data/ProcessedGraphs/";
    // -mappings argument for the path to store the mappings
    std::string mappings_path = "../Data/Mappings/";
    // -t arguments for the threads to use
    int num_threads = 1;
    // -method
    auto method = ged::Options::GEDMethod::F1;
    // -cost
    auto cost = ged::Options::EditCosts::CONSTANT;
    // -graph_ids
    std::string graph_ids_arg = "all";
    graph_ids_arg = "../Data/graph_ids.txt";
    std::vector<std::pair<INDEX, INDEX>> graph_ids;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-raw") {
            input_path = argv[i+1];
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
        else if (std::string(argv[i]) == "-method") {
            if (std::string(argv[i+1]) == "F1") {
                method = ged::Options::GEDMethod::F1;
            }
        }
        else if (std::string(argv[i]) == "-cost") {
            if (std::string(argv[i+1]) == "CONSTANT") {
                cost = ged::Options::EditCosts::CONSTANT;
            }
        }
        else if (std::string(argv[i]) == "-graph_ids") {
            // if argument is 'all'
            if (std::string(argv[i+1]) == "all") {
                graph_ids_arg = "all";
            }
            // if argument is path load the list to the ids to the graphs for which pairwise mappings are computed (list is one id per line no separations)
            else if (std::filesystem::exists(argv[i+1])) {
                graph_ids_arg = argv[i+1];
            }
            else {
                std::cout << "Graph ids file does not exist" << std::endl;
                return 1;
            }

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
        else {
            std::cout << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }

    }

    // create mapping output directory
    if (!std::filesystem::exists(mappings_path)) {
        std::filesystem::create_directory(mappings_path);
    }


    if (bool success = LoadSaveGraphDatasets::PreprocessTUDortmundGraphData(db, input_path, output_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData(db, output_path, graphs);


    if (graph_ids_arg == "all") {
        for (auto i = 0; i < graphs.graphData.size(); ++i) {
            for ( auto j = i + 1; j < graphs.graphData.size(); ++j) {
                graph_ids.emplace_back(i,j);
            }
        }
    }
    else {
        std::ifstream file(graph_ids_arg);
        std::string line;
        std::vector<int> ids;
        while (std::getline(file, line)) {
            int id = std::stoi(line);
            ids.push_back(id);
        }
        file.close();
        // add all possible pairs to graph ids
        for (auto id : ids) {
            for (auto id2 : ids) {
                if (id < id2) {
                    graph_ids.emplace_back(id, id2);
                }
            }
        }

    }

    // set omp number of threads to max threads of this machine
    const int chunk_size = std::min(100, static_cast<int>(graph_ids.size())/(2*num_threads));

    // shuffle the graph ids
    ranges::shuffle(graph_ids, std::mt19937{std::random_device{}()});
    std::vector<std::vector<std::pair<INDEX, INDEX>>> graph_id_chunks;
    // split graph ids into chunks of size chunk_size
    for (int i = 0; i < graph_ids.size(); i += chunk_size) {
        graph_id_chunks.emplace_back(graph_ids.begin() + i, graph_ids.begin() + min(i + chunk_size, static_cast<int>(graph_ids.size())));
    }


    // parallelize computation of ged using openmp
#pragma omp parallel for schedule(dynamic) shared(graphs, graph_id_chunks, mappings_path) default(none) num_threads(num_threads)
    for (const auto & graph_id_chunk : graph_id_chunks) {
        ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> env;
        InitializeGEDEnvironment(env, graphs, ged::Options::EditCosts::CONSTANT, ged::Options::GEDMethod::F1);
        ComputeGEDResults(env, graphs, graph_id_chunk, mappings_path);
    }
    std::string search_string = "_ged_mapping";
    MergeGEDResults(mappings_path, search_string, graphs);
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(mappings_path + db + "_ged_mapping.bin", graphs, results);
    CSVFromGEDResults(mappings_path + db + "_ged_mapping.csv", results);
    return 0;
}