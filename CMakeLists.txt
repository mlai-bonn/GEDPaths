cmake_minimum_required(VERSION 3.20)
project(GEDPaths)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

set(GEDLIB_ROOT ../libs/libGraph/external/gedlib)
set(LIBGRAPH_ROOT ../libs/libGraph/include)


include_directories(${LIBGRAPH_ROOT} ${GEDLIB_ROOT} ${GEDLIB_ROOT}/ext/boost.1.89.0 ${GEDLIB_ROOT}/ext/eigen.3.3.4/Eigen ${GEDLIB_ROOT}/ext/nomad.3.8.1/src ${GEDLIB_ROOT}/ext/nomad.3.8.1/ext/sgtelib/src ${GEDLIB_ROOT}/ext/lsape.5/cpp/include ${GEDLIB_ROOT}/ext/libsvm.3.22 ${GEDLIB_ROOT}/ext/fann.2.2.0/include)
link_directories(${LIBGRAPH_ROOT} ${GEDLIB_ROOT}/ext/nomad.3.8.1/lib ${GEDLIB_ROOT}/ext/libsvm.3.22 ${GEDLIB_ROOT}/ext/fann.2.2.0/lib )

# adding the Google_Tests_run target
add_executable(GEDPaths main.cpp ${LIBGRAPH_ROOT}/libGraph.h ${GEDLIB_ROOT}/src/env/ged_env.hpp
        load_tu.h)

target_link_libraries(GEDPaths libsvm.so libnomad.so libdoublefann.so.2)