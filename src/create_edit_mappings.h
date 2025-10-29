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

void create_edit_mappings();

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


#endif //GEDPATHS_CREATE_EDIT_MAPPINGS_H