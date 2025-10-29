// New file: analyze_mappings.cpp
// Load mappings for a given method and dataset and compare distances


#include "src/analyze_mappings.h"

int main(int argc, const char* argv[]) {
    // defaults
    std::string db = "MUTAG";
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    std::string mappings_root = "../Results/Mappings/";
    std::string method = "F2";
    std::string compare_method; // optional second method to compare against
    std::string csv_out; // optional CSV of pairwise comparisons

    // parse simple argv-style (consistent with repo tools)
    for (int i = 1; i < argc - 1; ++i) {
        std::string arg = argv[i];
        if (arg == "-db" || arg == "-data" || arg == "-dataset" || arg == "-database") {
            db = argv[i+1];
        } else if (arg == "-processed") {
            processed_graph_path = argv[i+1];
        } else if (arg == "-mappings") {
            mappings_root = argv[i+1];
        } else if (arg == "-method") {
            method = argv[i+1];
        } else if (arg == "-compare-method") {
            compare_method = argv[i+1];
        } else if (arg == "-csv-out") {
            csv_out = argv[i+1];
        } else if (arg == "-help") {
            std::cout << "analyze_mappings: load GED mappings and compare distances\n";
            std::cout << "Usage: " << argv[0] << " [-db NAME] [-method METHOD] [-compare-method OTHER_METHOD] [-mappings PATH] [-processed PATH] [-csv-out FILE]\n";
            return 0;
        }
    }

    return analyze_mappings(db, processed_graph_path, mappings_root, method, compare_method, csv_out);
}