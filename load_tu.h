//
// Created by florian on 04.09.25.
//

#ifndef GEDPATHS_LOAD_TU_H
#define GEDPATHS_LOAD_TU_H
#include <string>

inline bool create_tu( const std::string& dataset_name = "MUTAG", const std::string &input_path = "../Data/", const std::string &output_path = "../Data/ProcessedGraphs/")
{

    // check whether folder "../Data/dataset_name" exsits or is in Processed
    if (!std::filesystem::exists(input_path + dataset_name + "/") && !std::filesystem::is_directory(output_path + dataset_name))
    {
        std::cout << "Folder " << input_path << " does not exist" << std::endl;
        // Please create it and download the data dataset_name from link
        std::cout << "Please download the dataset from https://chrsmrrs.github.io/datasets/docs/datasets/" << std::endl;
        return false;
    }
    // if output_path except for ProcessedGraphs exists then create ProcessedGraphs folder
    if (!std::filesystem::exists(output_path)) {
        std::filesystem::create_directory(output_path);
    }



    // Check whether Processed graphs already exists
    if (std::filesystem::exists(output_path + dataset_name + ".bgfs")) {
        std::cout << "Graph " << dataset_name << " already exists" << std::endl;
        return true;
    }

    GraphStruct graphStruct;
    GraphData<GraphStruct> graphs;
    //graphs.add(example_graph());
    std::vector<int> graphLabels;
    std::vector<std::vector<int>> graphNodeLabels;
    std::vector<std::vector<int>> graphNodeAttributes;
    std::vector<std::vector<int>> graphEdgeAttributes;
    std::vector<std::vector<int>> graphEdgeLabels;
    LoadSave::LoadTUDortmundGraphData(input_path, dataset_name, graphs, graphLabels, &graphNodeLabels, &graphEdgeLabels, &graphNodeAttributes, &graphEdgeAttributes);

    SaveParams params = {
        output_path,
        dataset_name,
        GraphFormat::BGFS,
        true,
    };
    // Add information to graphs
    int counter = 0;
    for ( auto &x : graphs.graphData) {
        auto labels = Labels(graphNodeLabels[counter].begin(), graphNodeLabels[counter].end());
        std::string graph_name = dataset_name + "_" + std::to_string(counter);
        x.SetName(graph_name);
        x.set_labels(&labels);
        ++counter;
        // print counter
        std::cout << "Graph:" << counter << "/" << graphs.graphData.size() << std::endl;
        std::cout << x.nodes() << std::endl;
    }
    // Save the graph as bgfs format
    graphs.Save(params);

    return true;
}

inline GraphData<GraphStruct> load_tu(const std::string& dataset_name = "MUTAG", const std::string &output_path = "../Data/ProcessedGraphs/") {
    // Load the graph from the bgfs format

    GraphData<GraphStruct> loadedGraphs;
    std::string graph_path = output_path + dataset_name + ".bgfs";

    if (!std::filesystem::exists(graph_path)) {
        std::cout << "Graph " << dataset_name << " does not exist" << std::endl;
    }
    else {
        loadedGraphs.Load(graph_path);
        // print the loaded graphs
        for ( auto &x : loadedGraphs.graphData) {
            std::cout << x << std::endl;
        }
        std::cout << "Successfully loaded the " << dataset_name << " graphs from TUDataset" << std::endl;
    }
    loadedGraphs.SetName(dataset_name);
    return loadedGraphs;
}


#endif //GEDPATHS_LOAD_TU_H