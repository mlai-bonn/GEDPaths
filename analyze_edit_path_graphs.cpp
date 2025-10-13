//
// Created by florian on 10.10.25.
//

#include <Algorithms/GED/GEDFunctions.h>
#include <GraphDataStructures/GraphUndirectedFeaturedBase.h>


int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -edit_paths argument for the path to store the edit paths
    const std::string edit_path_output = "../Data/EditPaths/";

    // Load MUTAG edit paths
    GraphData<UDataGraph> edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>> edit_path_info;
    edit_paths.Load(edit_path_output + "MUTAG_edit_paths.bgf");
    std::string info_path = edit_path_output + "MUTAG_edit_paths_info.bin";
    ReadEditPathInfo(info_path, edit_path_info);
    return 0;
}