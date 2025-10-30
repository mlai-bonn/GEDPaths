//
// Created by florian on 28.10.25.
//

// define gurobi
#define GUROBI
// use gedlib
#define GEDLIB

#ifndef GEDPATHS_CREATE_EDIT_MAPPINGS_H
#define GEDPATHS_CREATE_EDIT_MAPPINGS_H
#include <utility>
#include <vector>
#include <libGraph.h>
#include <src/env/ged_env.hpp>

int create_edit_mappings(const std::string& db,
                            const std::string& output_path,
                            const std::string& input_path,
                            const std::string& processed_graph_path,
                          ged::Options::EditCosts edit_cost,
                          ged::Options::GEDMethod ged_method,
                          const std::string& method_options = "",
                          const std::string& graph_ids_path = "",
                          int num_pairs = -1,
                          int num_threads = 1,
                          int seed = 42,
                          int single_source = -1,
                          int single_target = -1);

GEDEvaluation<UDataGraph> create_edit_mappings_single(INDEX source_id, INDEX target_id, GraphData<UDataGraph>& graphs, ged::Options::EditCosts edit_cost, ged::Options::GEDMethod ged_method, const std::string& method_options = "", bool print = false);


inline GEDEvaluation<UDataGraph> create_edit_mappings_single(INDEX source_id, INDEX target_id, GraphData<UDataGraph>& graphs, ged::Options::EditCosts edit_cost, ged::Options::GEDMethod ged_method, const std::string& method_options, bool print) {
    if (source_id >= graphs.graphData.size() || target_id >= graphs.graphData.size()) {
        std::cerr << "Single source/target IDs out of range: " << source_id << ", " << target_id << std::endl;
        exit(1);
    }
    std::pair<INDEX, INDEX> pair = std::minmax(source_id, target_id);
    std::vector<std::pair<INDEX, INDEX>> single_pair{pair};
    auto ged_env = ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>();
    InitializeGEDEnvironment(ged_env, graphs, edit_cost, ged_method, method_options);
    std::vector<GEDEvaluation<UDataGraph>> results;
    ged_env.run_method(pair.first, pair.second);
    GEDEvaluation<UDataGraph> result = ComputeGEDResult(ged_env, graphs, pair.first, pair.second);
    if (print) {
        // Print result for debugging
        std::cout << "Computed mapping for pair (" << pair.first << ", " << pair.second << ")" << std::endl;
        // Optionally print more details if available
        std::cout << "Distance: " << result.distance << std::endl;
        std::cout << "Lower Bound: " << result.lower_bound << std::endl;
        std::cout << "Upper Bound: " << result.upper_bound << std::endl;
        // also return the mapping
        std::cout << "Node Mapping (source -> target):" << std::endl;
        for (INDEX i = 0; i < result.node_mapping.first.size(); ++i) {
            std::cout << "  " << i << " -> " << result.node_mapping.first[i] << std::endl;
        }
        std::cout << "  Target to Source:" << std::endl;
        for (INDEX i = 0; i < result.node_mapping.second.size(); ++i) {
            std::cout << "  " << i << " -> " << result.node_mapping.second[i] << std::endl;
        }
    }
    return result;

}


inline void fixInvalidMappings(std::vector<GEDEvaluation<UDataGraph>>& results,
                               GraphData<UDataGraph>& graphs,
                               ged::Options::EditCosts edit_cost,
                               ged::Options::GEDMethod ged_method,
                               const std::string& method_options) {

    // try to correct invalid mappings
    auto invalid_mappings = CheckResultsValidity(results);
    std::cout << "Found " << invalid_mappings.size() << " invalid mappings.\n";
    if (invalid_mappings.empty()) {
        return;
    }

    // recalulate the mappings for the invalid results
    std::cout << "Recalculating mappings for invalid results...\n";
    std::vector<std::pair<INDEX, GEDEvaluation<UDataGraph>>> fixed_results;
    for (const auto &id : invalid_mappings) {
        auto source_id = results[id].graph_ids.first;
        auto target_id = results[id].graph_ids.second;
        auto fixed_result = create_edit_mappings_single(source_id, target_id, graphs, edit_cost, ged_method, method_options, false);
        if (CheckResultsValidity(std::vector<GEDEvaluation<UDataGraph>>{fixed_result}).empty()) {
            fixed_results.emplace_back(id, fixed_result);
            std::cout << "  Fixed mapping for result id " << id << " (Graph IDs: " << source_id << ", " << target_id << ")\n";
        }
    }
    // replace invalid results with fixed results
    for (auto &[id, result] : fixed_results) {
        results[id] = result;
    }
}

inline int create_edit_mappings(const std::string& db,
                                const std::string& output_path,
                                const std::string& input_path,
                                const std::string& processed_graph_path,
                                ged::Options::EditCosts edit_cost,
                                ged::Options::GEDMethod ged_method,
                                const std::string& method_options,
                                const std::string& graph_ids_path,
                                int num_pairs,
                                int num_threads,
                                int seed,
                                int single_source,
                                int single_target) {
    if (const bool success = LoadSaveGraphDatasets::PreprocessTUDortmundGraphData(db, input_path, processed_graph_path); !success) {
        std::cout << "Failed to create TU dataset" << std::endl;
        return 1;
    }
    GraphData<UDataGraph> graphs;
    LoadSaveGraphDatasets::LoadPreprocessedTUDortmundGraphData(db, processed_graph_path, graphs);
    std::vector<std::pair<INDEX, INDEX>> graph_ids;
    // set up random device
    auto gen = std::mt19937_64(seed);
        // If db_ged_mapping.bin already exists load it and look for existing graph ids
    // load mappings
    std::vector<GEDEvaluation<UDataGraph>> results;
    std::string mapping_file =output_path + "/" + db + "/" + db + "_ged_mapping.bin";
    std::vector<std::pair<INDEX, INDEX>> existing_graph_ids;
    if (std::filesystem::exists(mapping_file)) {
        BinaryToGEDResult(mapping_file, graphs, results);
        for (const auto& res : results) {
            existing_graph_ids.emplace_back(res.graph_ids);
        }
        // sort existing graph ids
        ranges::sort(existing_graph_ids, [](const std::pair<INDEX, INDEX>& a, const std::pair<INDEX, INDEX>& b) {
            return a.first == b.first ? a.second < b.second : a.first < b.first;
        });
        // fix invalid mappings
        fixInvalidMappings(results, graphs, edit_cost, ged_method, method_options);
        // save the updated results back to binary
        GEDResultToBinary(output_path + "/" + db + "/", results);
    }

    // If single_source and single_target are set, only compute and print that mapping
    if (single_source >= 0 && single_target >= 0) {
        auto result = create_edit_mappings_single(single_source, single_target, graphs, edit_cost, ged_method, method_options, true);
        return 0;
    }

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

    //sort graph_ids
    std:ranges::sort(graph_ids
                     ,
                     [](const std::pair<INDEX, INDEX>& a, const std::pair<INDEX, INDEX>& b) {
                         return a.first == b.first ? a.second < b.second : a.first < b.first;
                     }
        );

    // remove all entries in graph_ids that also occur in existing_graph_ids (use that both are sorted)
    std::erase_if(
        graph_ids,
        [&existing_graph_ids](const std::pair<INDEX, INDEX>& pair) {
            return ranges::binary_search(existing_graph_ids, pair,
                                         [](const std::pair<INDEX, INDEX>& a, const std::pair<INDEX, INDEX>& b) {
                                             return a.first == b.first ? a.second < b.second : a.first < b.first;
                                         });
        }
    );
    // std::cout number of pairs to compute
    std::cout << "Number of GED mappings to compute: " << graph_ids.size() << std::endl;

    // Ensure base tmp directory exists
    std::filesystem::path base_tmp = output_path + db + "/tmp/";
    std::filesystem::create_directories(base_tmp);

    // If threads is one do not chunk
    int threads = num_threads > 0 ? num_threads : 1;
    const size_t total_pairs = graph_ids.size();
    if (threads == 1) {
        auto ged_env = ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>();
        InitializeGEDEnvironment(ged_env, graphs, edit_cost, ged_method, method_options);
        ComputeGEDResults(ged_env, graphs, graph_ids, base_tmp.string(), ged_method, method_options);
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
                ComputeGEDResults(*env_tls, graphs, chunks[ci], chunk_tmp.string() + "/", ged_method, method_options);

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
    results.clear();
    BinaryToGEDResult(output_path + db + "/" + db + "_ged_mapping.bin", graphs, results);
    // Fix invalid mappings that are still present (due to parallel execution issues in gedlib)
    fixInvalidMappings(results, graphs, edit_cost, ged_method, method_options);
    // save the updated results back to binary
    GEDResultToBinary(output_path + "/" + db + "/", results);
    CSVFromGEDResults(output_path + db + "/" + db + "_ged_mapping.csv", results);

    return 0;
}

#endif //GEDPATHS_CREATE_EDIT_MAPPINGS_H