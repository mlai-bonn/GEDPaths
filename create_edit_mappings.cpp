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
// OpenMP for parallel execution
#include <omp.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <iomanip>

// source_id and target_id as args
int main(int argc, const char * argv[]) {
    // -db argument for the database (accepts several synonyms)
    std::string db = "MUTAG";
    // -raw argument for the raw data path
    std::string input_path = "../Data/Graphs/";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -mappings argument for the path to store the mappings
    std::string output_path = "../Results/Mappings/";
    // -t arguments for the threads to use
    int num_threads = 1;
    // -method
    auto method = "F2";
    std::string method_options = "";
    auto ged_method = GEDMethodFromString(method);
    // -cost
    auto cost = "CONSTANT";
    auto edit_cost = EditCostsFromString(cost);
    // -s
    auto seed = 42;
    // -ids_path
    std::string graph_ids_path;
    // -num_pairs to randomly sample from the dataset and create mappings for (-1 for all)
    int num_pairs = 5000;
    std::vector<std::pair<INDEX, INDEX>> graph_ids;

    // Add single source/target arguments
    int single_source = -1;
    int single_target = -1;

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-db" || std::string(argv[i]) == "-data" || std::string(argv[i]) == "-dataset" || std::string(argv[i]) == "-database") {
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
        else if (std::string(argv[i]) == "-method_options") {
            // read method options in format option value option value ... until next  leading - or end of argv
            int counter = 0;
            while (i + 1 < argc && std::string(argv[i+1]).rfind('-', 0) != 0) {
                // if current argv is option add -- prefix otherwise just add value
                if (counter % 2 == 0) {
                    method_options += "--" + std::string(argv[i+1]) + " ";
                }
                else {
                    method_options += std::string(argv[i+1]) + " ";
                }
                counter++;
                i++;
            }
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
        else if (std::string(argv[i]) == "-single_source") {
            single_source = std::stoi(argv[i+1]);
        }
        else if (std::string(argv[i]) == "-single_target") {
            single_target = std::stoi(argv[i+1]);
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            std::cout << "Create edit mappings for a given database/dataset" << std::endl;
            std::cout << "Arguments:" << std::endl;
            std::cout << "-db | -data | -dataset | -database <database name>" << std::endl;
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
    auto gen = std::mt19937_64(seed);


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

    // If single_source and single_target are set, only compute and print that mapping
    if (single_source >= 0 && single_target >= 0) {
            auto result = create_edit_mappings_single(single_source, single_target, graphs, edit_cost, ged_method, method_options, true);
           return 0;
    }
    // ...existing code...
    else {
        if (num_pairs > 0 && num_pairs <= graphs.graphData.size() * (graphs.graphData.size() - 1) / 2) {
            std::uniform_int_distribution<INDEX> dist(0, graphs.graphData.size() - 1);
            while (graph_ids.size() < num_pairs) {
                // get random integer between 0 and graphs.GraphData.size() - 1
                INDEX id1 = dist(gen);
                // id2 should between id1 + 1 and graphs.GraphData.size() - 1
                INDEX id2 = dist(gen);
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

    // Ensure base tmp directory exists
    std::filesystem::path base_tmp = output_path + db + "/tmp/";
    std::filesystem::create_directories(base_tmp);

    // If threads is one do not chunk
    int threads = num_threads > 0 ? num_threads : 1;
    const size_t total_pairs = graph_ids.size();
    if (threads == 1) {
        auto ged_env = ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>();
        InitializeGEDEnvironment(ged_env, graphs, edit_cost, ged_method, method_options);
        ComputeGEDResults(ged_env, graphs, graph_ids, base_tmp.string());
    }
    else {
        // Parallel execution: split graph_ids into more, smaller chunks
        int num_chunks = std::max(1, threads * 10); // threads*10 chunks gives finer granularity
        size_t chunk_size = (total_pairs + num_chunks - 1) / num_chunks;
        if (chunk_size == 0) chunk_size = 1;

        std::vector<std::vector<std::pair<INDEX, INDEX>>> chunks;
        for (size_t i = 0; i < total_pairs; i += chunk_size) {
            size_t end = std::min(total_pairs, i + chunk_size);
            chunks.emplace_back(graph_ids.begin() + i, graph_ids.begin() + end);
        }

        // Parallel processing of chunks. Each thread creates its own GEDEnv and writes to a thread-local tmp dir.
        std::atomic<int> finished_chunks{0};
        const int total_chunks = static_cast<int>(chunks.size());
        const auto start_time = std::chrono::steady_clock::now();
        #pragma omp parallel for schedule(dynamic) num_threads(threads)
        for (int ci = 0; ci < total_chunks; ++ci) {
            try {
                // Use a thread-local GEDEnv so that a thread reuses its environment across multiple chunks
                thread_local std::unique_ptr<ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>> env_tls;
                if (!env_tls) {
                    env_tls = std::make_unique<ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>>();
                    InitializeGEDEnvironment(*env_tls, graphs, edit_cost, ged_method, method_options);
                }

                // Create per-thread-per-chunk tmp directory to avoid collisions (a thread may handle multiple chunks)
                int tid = omp_get_thread_num();
                std::filesystem::path thread_tmp = base_tmp / ("thread_" + std::to_string(tid));
                std::filesystem::path chunk_tmp = thread_tmp / ("chunk_" + std::to_string(ci));
                std::filesystem::create_directories(chunk_tmp);

                // Compute GED results for this chunk into the chunk-specific tmp directory
                ComputeGEDResults(*env_tls, graphs, chunks[ci], chunk_tmp.string() + "/");

                // Progress reporting (atomic)
                int done = ++finished_chunks;
                if (done % std::max(1, total_chunks / 100) == 0 || done == total_chunks) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time).count();
                    double rate = elapsed > 1e-9 ? static_cast<double>(done) / elapsed : 0.0; // chunks/s
                    double pct = (100.0 * done) / total_chunks;
                    double remaining_seconds = rate > 1e-9 ? (static_cast<double>(total_chunks - done) / rate) : -1.0;

                    auto format_seconds = [](double secs) -> std::string {
                        if (secs < 0) return std::string("unknown");
                        int s = static_cast<int>(std::round(secs));
                        int h = s / 3600;
                        int m = (s % 3600) / 60;
                        int ss = s % 60;
                        std::ostringstream oss;
                        if (h > 0) {
                            oss << h << ":" << std::setw(2) << std::setfill('0') << m << ":" << std::setw(2) << std::setfill('0') << ss;
                        } else {
                            oss << m << ":" << std::setw(2) << std::setfill('0') << ss;
                        }
                        return oss.str();
                    };

#pragma omp critical (progress_print)
                    {
                        std::cout << std::fixed << std::setprecision(1)
                                  << "Progress: " << done << "/" << total_chunks << " chunks (" << pct << "%), "
                                  << "elapsed=" << std::setprecision(1) << elapsed << "s, rate=" << std::setprecision(2) << rate << " chunks/s"
                                  << ", ETA=" << format_seconds(remaining_seconds) << "\n";
                    }
                }
            } catch (const std::exception &e) {
                std::cerr << "Exception in parallel GED computation for chunk " << ci << ": " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in parallel GED computation for chunk " << ci << std::endl;
            }
        }
    }


    std::string search_string = "_ged_mapping";
    MergeGEDResults(output_path + db + "/tmp/", output_path + db + "/", search_string, graphs);
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    BinaryToGEDResult(output_path + db + "/" + db + "_ged_mapping.bin", graphs, results);
    CSVFromGEDResults(output_path + db + "/" + db + "_ged_mapping.csv", results);
    return 0;
}
