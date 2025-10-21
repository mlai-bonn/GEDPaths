message("$ENV{GUROBI_HOME}")
message("${GUROBI_DIR}")
find_library(
        GUROBI_LIBRARY
        NAMES gurobi gurobi81 gurobi90 gurobi95 gurobi100 gurobi110 gurobi120 gurobi1203 gurobi12
        HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME}
        PATH_SUFFIXES lib)
message("${GUROBI_LIBRARY}")
find_library(
        GUROBI_CXX_LIBRARY
        NAMES gurobi_c++
        HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME}
        PATH_SUFFIXES lib)
message("${GUROBI_CXX_LIBRARY}")
find_path(
        GUROBI_INCLUDE_DIRS
        NAMES gurobi_c.h
        HINTS ${GUROBI_DIR} $ENV{GUROBI_HOME}
        PATH_SUFFIXES include)
message("${GUROBI_INCLUDE_DIRS}")
include(FindPackageHandleStandardArgs) # include the "FindPackageHandleStandardArgs" module
find_package_handle_standard_args(GUROBI DEFAULT_MSG GUROBI_LIBRARY GUROBI_CXX_LIBRARY GUROBI_INCLUDE_DIRS)

if (GUROBI_FOUND)
    add_library(gurobi STATIC IMPORTED)
    set_target_properties(gurobi PROPERTIES IMPORTED_LOCATION ${GUROBI_CXX_LIBRARY})
    target_link_libraries(gurobi INTERFACE ${GUROBI_LIBRARY})
    target_include_directories(gurobi INTERFACE ${GUROBI_INCLUDE_DIRS})
endif()
