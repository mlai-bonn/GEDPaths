//
// Created by florian on 10.10.25.
//

#include <Algorithms/GED/GEDFunctions.h>
#include <GraphDataStructures/GraphUndirectedFeaturedBase.h>


#include <numeric>
#include <cmath>
#include <limits>
#include <tuple>
#include <iostream>
#include <vector>
#include <map>

struct BucketOperations {
    unsigned long _node_insertions = 0;
    unsigned long _node_deletions = 0;
    unsigned long _node_relabels = 0;
    unsigned long _edge_insertions = 0;
    unsigned long _edge_deletions = 0;
    unsigned long _edge_relabels = 0;
};

// Statistic class for num, average, stddev, min, max of a list of values with name
class ValueStatistics {
public:
    ValueStatistics()= default;
    explicit ValueStatistics(const std::string& name, const std::vector<double>& values);
    void PrintStatistics() const;
private:
    std::string _name;
    std::vector<double> _values;
    unsigned long _num_values = 0;
    double _sum_values = 0.0;
    double _average = 0.0;
    double _stddev = 0.0;
    double _min = std::numeric_limits<double>::max();
    double _max = std::numeric_limits<double>::min();
};

ValueStatistics::ValueStatistics(const std::string &name, const std::vector<double> &values) {
    _name = name;
    _values = values;
    _num_values = values.size();
    _sum_values = std::accumulate(values.begin(), values.end(), 0.0);
    if (_num_values > 0) {
        _average = _sum_values / _num_values;
        double sum_squared_diff = 0.0;
        for (const auto& value : values) {
            sum_squared_diff += (value - _average) * (value - _average);
            if (value < _min) {
                _min = value;
            }
            if (value > _max) {
                _max = value;
            }
        }
        _stddev = std::sqrt(sum_squared_diff / _num_values);
    } else {
        _min = 0.0;
        _max = 0.0;
    }
}

void ValueStatistics::PrintStatistics() const {
    std::cout << "Statistics for " << _name << ":\n";
    std::cout << "  Number of values: " << _num_values << "\n";
    std::cout << "  Average: " << _average << "\n";
    std::cout << "  Standard Deviation: " << _stddev << "\n";
    std::cout << "  Minimum: " << _min << "\n";
    std::cout << "  Maximum: " << _max << "\n";
}



// Statistic class for edit paths
class EditPathStatistics {
public:
    EditPathStatistics()= default;
    explicit EditPathStatistics(const GraphData<UDataGraph>& edit_paths, const std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>>& edit_path_info);
    void PrintStatistics() const;
private:
    GraphData<UDataGraph> _edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>> _edit_path_info;
    ValueStatistics _num_nodes_stats;
    ValueStatistics _num_edges_stats;
    ValueStatistics _num_operations_stats;
    ValueStatistics _path_length_stats;
    ValueStatistics _node_insertions_stats;
    ValueStatistics _node_deletions_stats;
    ValueStatistics _node_relabels_stats;
    ValueStatistics _edge_insertions_stats;
    ValueStatistics _edge_deletions_stats;
    ValueStatistics _edge_relabels_stats;
    ValueStatistics _connectedness_stats;
};

EditPathStatistics::EditPathStatistics(const GraphData<UDataGraph> &edit_paths, const std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>> &edit_path_info)
    : _edit_paths(edit_paths), _edit_path_info(edit_path_info) {
    std::vector<double> num_nodes;
    std::vector<double> num_edges;
    std::vector<double> num_operations;
    std::vector<double> path_lengths;
    std::vector<double> node_insertions;
    std::vector<double> node_deletions;
    std::vector<double> node_relabels;
    std::vector<double> edge_insertions;
    std::vector<double> edge_deletions;
    std::vector<double> edge_relabels;
    const unsigned long bucket_size = 10;
    std::vector<BucketOperations> buckets ( bucket_size );

    // Map to count operations per (source_id, target_id)
    std::map<std::pair<INDEX, INDEX>, std::vector<EditOperation>> operations_map;

    for (const auto& entry : _edit_path_info) {
        INDEX source_id = std::get<0>(entry);
        INDEX step_id = std::get<1>(entry);
        INDEX target_id = std::get<2>(entry);
        EditOperation operation = std::get<3>(entry);
        operations_map[{source_id, target_id}].push_back(operation);
    }

    // Now calculate statistics based on operations_map
    int counter = 0;
    for (const auto& [key, operations] : operations_map) {
        INDEX source_id = key.first;
        INDEX target_id = key.second;

        // Find all graphs corresponding to the current path order in operations_map is the same as in edit_paths
        std::vector<UDataGraph*> path_graphs = std::vector<UDataGraph*>(operations.size() + 1, nullptr);
        for (int i = 0; i < path_graphs.size(); ++i) {
            path_graphs[i] = &_edit_paths.graphData[counter + i];
        }

        if (!path_graphs.empty()) {
            for (const auto& g : path_graphs) {
                num_nodes.push_back(static_cast<double>(g->nodes()));
                num_edges.push_back(static_cast<double>(g->edges()));
            }
            node_insertions.push_back(0.0);
            node_deletions.push_back(0.0);
            node_relabels.push_back(0.0);
            edge_insertions.push_back(0.0);
            edge_deletions.push_back(0.0);
            edge_relabels.push_back(0.0);

            unsigned long bucket_counter = 0;
            unsigned long operation_counter = 0;
            for (const auto& op : operations) {
                bucket_counter = std::min(static_cast<unsigned long>(std::floor(static_cast<double>(operation_counter) / (operations.size() / static_cast<double>(bucket_size)))), bucket_size - 1);
                switch (op.operationObject) {
                    case OperationObject::NODE:
                        if (op.type == EditType::INSERT) {
                            node_insertions.back() += 1.0;
                            buckets[bucket_counter]._node_insertions += 1;
                        } else if (op.type == EditType::DELETE) {
                            node_deletions.back() += 1.0;
                            buckets[bucket_counter]._node_deletions += 1;
                        } else if (op.type == EditType::RELABEL) {
                            node_relabels.back() += 1.0;
                            buckets[bucket_counter]._node_relabels += 1;
                        }
                        break;
                    case OperationObject::EDGE:
                        if (op.type == EditType::INSERT) {
                            edge_insertions.back() += 1.0;
                            buckets[bucket_counter]._edge_insertions += 1;
                        } else if (op.type == EditType::DELETE) {
                            edge_deletions.back() += 1.0;
                            buckets[bucket_counter]._edge_deletions += 1;
                        } else if (op.type == EditType::RELABEL) {
                            edge_relabels.back() += 1.0;
                            buckets[bucket_counter]._edge_relabels += 1;
                        }
                        break;
                    default:
                        break;
                }
                operation_counter += 1;
            }
            num_operations.push_back(static_cast<double>(operations.size()));
            path_lengths.push_back(static_cast<double>(operations.size()));
        }
        counter += operations.size();
    }
    _num_nodes_stats = ValueStatistics("Number of Nodes", num_nodes);
    _num_edges_stats = ValueStatistics("Number of Edges", num_edges);
    _num_operations_stats = ValueStatistics("Number of Operations", num_operations);
    _path_length_stats = ValueStatistics("Path Length", path_lengths);
    _node_insertions_stats = ValueStatistics("Node Insertions", node_insertions);
    _node_deletions_stats = ValueStatistics("Node Deletions", node_deletions);
    _node_relabels_stats = ValueStatistics("Node Relabels", node_relabels);
    _edge_insertions_stats = ValueStatistics("Edge Insertions", edge_insertions);
    _edge_deletions_stats = ValueStatistics("Edge Deletions", edge_deletions);
    _edge_relabels_stats = ValueStatistics("Edge Relabels", edge_relabels);


}
void EditPathStatistics::PrintStatistics() const {
    std::cout << "Edit Path Statistics:\n";
    _num_nodes_stats.PrintStatistics();
    _num_edges_stats.PrintStatistics();
    _num_operations_stats.PrintStatistics();
    _path_length_stats.PrintStatistics();
    _node_insertions_stats.PrintStatistics();
    _node_deletions_stats.PrintStatistics();
    _node_relabels_stats.PrintStatistics();
    _edge_insertions_stats.PrintStatistics();
    _edge_deletions_stats.PrintStatistics();
    _edge_relabels_stats.PrintStatistics();
}


int main(int argc, const char * argv[]) {
    // -db argument for the database
    std::string db = "MUTAG";
    // -processed argument for the processed data path
    std::string processed_graph_path = "../Data/ProcessedGraphs/";
    // -edit_paths base argument for the path to store the edit paths
    std::string edit_path_output = "../Results/Paths/";
    // -method (default REFINE)
    std::string method = "REFINE";

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-db") {
            db = argv[i+1];
        }
        else if (std::string(argv[i]) == "-processed") {
            processed_graph_path = argv[i+1];
        }
        else if (std::string(argv[i]) == "-method") {
            method = argv[i+1];
        }
        // add help
        else if (std::string(argv[i]) == "-help") {
            // TODO
             return 0;
        }
        else {
            std::cout << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }

    }
    std::string edit_path_output_db = edit_path_output + method + "/" + db + "/";

    // Load MUTAG edit paths
    GraphData<UDataGraph> edit_paths;
    std::vector<std::tuple<INDEX, INDEX, INDEX, EditOperation>> edit_path_info;
    edit_paths.Load(edit_path_output_db + db + "_edit_paths.bgf");
    std::string info_path = edit_path_output_db + db + "_edit_paths_info.bin";
    ReadEditPathInfo(info_path, edit_path_info);
    EditPathStatistics stats(edit_paths, edit_path_info);
    stats.PrintStatistics();


    return 0;
}