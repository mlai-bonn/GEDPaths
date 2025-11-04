//
// Created by florian on 10.10.25.
//


#include "src/analyze_edit_path_graphs.h"
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -edit_paths base argument for the path to store the edit paths
    std::string edit_path_output = "../Results/Paths/";
    // path generation strategy
    std::string path_generation_strategy = "Rnd_d-IsoN";
    std::string method = "F2";

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-db" || std::string(argv[i]) == "-data" || std::string(argv[i]) == "-dataset" || std::string(argv[i]) == "-database") {
            db = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-method") {
            method = argv[i+1];
            ++i;
        }
        else if (std::string(argv[i]) == "-path_strategy") {
            path_generation_strategy = argv[i+1];
            ++i;
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            // TODO
            std::cout << "Analyze edit path statistics" << std::endl;
            std::cout << "Arguments:" << std::endl;
            std::cout << "-db | -data | -dataset | -database <database name>" << std::endl;
            std::cout << "-processed <processed data path>" << std::endl;
            std::cout << "-method <GED method name>" << std::endl;
            std::cout << "-path_strategy <single strategy name>" << std::endl;
            std::cout << "-path_strategies <comma,separated,list,of,strategies>" << std::endl;
             return 0;
        }
        else {
            std::cout << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }

    }

    // in edit_path_output search for Paths/ and replace this by Paths_<path_generation_strategy>/
    size_t pos = edit_path_output.find("Paths/");
    if (pos != std::string::npos) {
        edit_path_output.replace(pos, 6, "Paths_" + path_generation_strategy + "/");
    } else {
        edit_path_output += "Paths_" + path_generation_strategy + "/";
    }


    return analyze_edit_path_graphs(db, edit_path_output, method);
}
