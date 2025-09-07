//
// Created by florian on 05.09.25.
//

#ifndef GEDPATHS_CREATE_EDIT_PATHS_H
#define GEDPATHS_CREATE_EDIT_PATHS_H

// This file shows how to use the libGraph graph library


#include "SimplePatterns.h"
#include "Algorithms/GED/GEDFunctions.h"
#include "GraphDataStructures/GraphBase.h"
#include "src/env/ged_env.hpp"

// main function to run the examples

void add_graph_to_env(ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>& env, const GraphStruct& g) {
    // Add graph
    env.add_graph(g.GetName());
    // Add nodes
    // get last env graph id
    std::pair<ged::GEDGraph::GraphID, ged::GEDGraph::GraphID> graph_ids = env.graph_ids();
    for (int i = 0; i < g.nodes(); ++i) {
        if (g.labelType == LABEL_TYPE::UNLABELED) {
            env.add_node(graph_ids.second - 1, i, 0);
        }
        else {
            env.add_node(graph_ids.second - 1, i, g.label(i));
        }
    }
    // Add edges
    for (int i = 0; i < g.nodes(); ++i) {
        for (int j = i + 1; j < g.nodes(); ++j) {
            env.add_edge(graph_ids.second - 1, i, j,0);
        }
    }
}

inline GEDResult result_from_env(ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>& env, GraphData<GraphStruct>& graphs, const int source_graph_id = 0, const int target_graph_id = 1){
    const ged::NodeMap& node_map = env.get_node_map(source_graph_id, target_graph_id);
    std::pair<Nodes, Nodes> mapping;
    for (const auto x : node_map.get_forward_map()) {
        mapping.first.push_back(x);
    }
    for (const auto x : node_map.get_backward_map()) {
        mapping.second.push_back(x);
    }

    GEDResult result = {
        node_map.induced_cost(),
        {graphs[source_graph_id], graphs[target_graph_id]},
        mapping,
        env.get_runtime(source_graph_id,target_graph_id),
    };
    return result;
}

inline void initialize_env(ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>& env, const GraphData<GraphStruct>& graph_data) {

    for (const auto& g : graph_data.graphData) {
        add_graph_to_env(env, g);
    }
    env.init();
}


inline std::vector<GraphStruct> pairwise_path(ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID>& env, GraphData<GraphStruct>& graph_data, const int source_id = 0, const int target_id = 1)
    {
        env.run_method(source_id,target_id);
        GEDResult result = result_from_env(env, graph_data, source_id, target_id);
        std::cout << "Approximated Distance: " << result.distance << std::endl;
        std::cout << "Time: " << result.time << " seconds" << std::endl;
        std::cout << "Quasimetric Cost: " << env.quasimetric_costs() << std::endl;
        // print node mapping
        //std::cout << "Node Mapping First: " << std::endl;
        //for (const auto& x : result.node_mapping.first) {
        //    std::cout << x << " ";
        //}
        //std::cout << std::endl;
        //std::cout << "Node Mapping Second: " << std::endl;
        //for (const auto& x : result.node_mapping.second) {
        //    std::cout << x << " ";
        //}
        //std::cout << std::endl;

        EditPath edit_path;
        result.get_edit_path(edit_path, 0);
        // print edit path length
        std::cout << "Edit Path Length: " << edit_path.edit_path_graphs.size() - 1 << std::endl;
        std::vector<GraphStruct> edit_path_graphs;
        for (const auto& g : edit_path.edit_path_graphs) {
            edit_path_graphs.emplace_back(g);
        }
        edit_path_graphs.back().SetName(edit_path.target_graph.GetName());
        // print all the graphs
        //for (const auto& g : edit_path_graphs.graphData) {
        //    std::cout << g << std::endl;
        //}
    return edit_path_graphs;
}

inline void compute_all_pairwise_paths(ged::GEDEnv<ged::LabelID, ged::LabelID, ged::LabelID> &env,
                                       GraphData<GraphStruct> &graph_data,
                                       const std::string &edit_path_output = "../Data/EditPaths/") {
    // check whether file already exists
    if (std::filesystem::exists(edit_path_output + graph_data.GetName() + "_edit_paths.bgf")) {
        std::cout << "Edit paths for " << graph_data.GetName() << " already exist." << std::endl;
        return;
    }
    // create tmp directory
    std::filesystem::create_directory(edit_path_output + "tmp/");
    // counter for number of computed paths
   int counter = 0;
    // time variable
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    for (int i = 0; i < graph_data.size(); ++i) {
        std::cout << graph_data[i] << std::endl;
        GraphData<GraphStruct> result;
        for (int j = i+1; j < graph_data.size(); ++j) {
            std::cout << "Computing Path between graph " << i << " and graph " << j << std::endl;
            // print percentage
            std::cout << "Progress: " << (counter * 100) / (graph_data.size() * (graph_data.size() - 1) / 2) << "%" << std::endl;
            // estimated time in minutes
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            const double elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
            const double estimated_total_time = (elapsed_seconds / (counter + 1)) * (graph_data.size() * (graph_data.size() - 1) / 2);
            const double estimated_time_left = estimated_total_time - elapsed_seconds;
            std::cout << "Estimated time left: " << estimated_time_left / 60 << " minutes" << std::endl;
            int path_counter = 0;
            for (std::vector<GraphStruct> new_result = pairwise_path(env, graph_data, i, j); auto& g : new_result) {
                g.SetName(graph_data.GetName() + "_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(path_counter));
                result.add(g);
                ++path_counter;
            }
            ++counter;
        }
        // save intermediate result in the tmp folder under datasetname_i.bgfs
        SaveParams params = {
            edit_path_output + "tmp/",
            graph_data.GetName() + "_" + std::to_string(i),
            GraphFormat::BGF,
            true,
        };
        result.Save(params);
        std::cout << "Saved intermediate result for graph " << i << std::endl;
    }
    // Merge all graphs in tmp folder
    GraphData<GraphStruct> final_result;
    for (int i = 0; i < graph_data.size(); ++i) {
        GraphData<GraphStruct> result;
        std::string file_path = edit_path_output + "tmp/" + graph_data.GetName() + "_" + std::to_string(i) + ".bgf";
        result.Load(file_path);
        for (const auto& g : result.graphData) {
            final_result.add(g);
        }
    }
    // Save final result
    SaveParams params = {
        edit_path_output,
        graph_data.GetName() + "_edit_paths",
        GraphFormat::BGF,
        true,
    };
    final_result.Save(params);
    // remove all databasename related files in tmp folder
    for (const auto& entry : std::filesystem::directory_iterator(edit_path_output + "tmp/")) {
        if (entry.path().string().find(graph_data.GetName() + "_") != std::string::npos) {
            std::filesystem::remove(entry.path());
        }
    }
}



#endif //GEDPATHS_CREATE_EDIT_PATHS_H