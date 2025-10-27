//
// Created by florian on 27.10.25.
//

#include <string>
#include <iostream>
#include "libGraph.h"
// load the bgf graph data from file
int main()
{
    std::string bgf_path = "../Results/Paths/F2/MUTAG/MUTAG_edit_paths.bgf";
    GraphData<UDataGraph> edit_paths;
    edit_paths.Load(bgf_path);
    std::cout << "Loaded " << edit_paths.size() << " edit paths from " << bgf_path << std::endl;
    return 0;
}