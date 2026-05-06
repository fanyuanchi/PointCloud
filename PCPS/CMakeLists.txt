cmake_minimum_required(VERSION 3.27)
project(PCPS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/util/libtorch")
find_package(Torch REQUIRED)

add_executable(PCPS main.cpp
        util/UTIL.h
        util/UTIL.cpp
        index/IIndex.h
        index/Geometry.h
        index/Geometry.cpp
        index/BruteForce/Brute.h
        index/BruteForce/Brute.cpp
        index/R-BVH/R-BVH.h
        index/R-BVH/R-BVH.cpp
        core/Worker.h
        core/Worker.cpp
        core/PCPS.h
        core/PCPS.cpp
        index/R-TreeFamily/R-TreeTemplate.cpp
        index/R-TreeFamily/R-TreeTemplate.h
        index/R-TreeFamily/RRS-Tree/RRS-Tree.cpp
        index/R-TreeFamily/RRS-Tree/RRS-Tree.h
        index/R-TreeFamily/R-Tree/R-Tree.h
        index/R-TreeFamily/R-Tree/R-Tree.cpp
        index/R-TreeFamily/RS-Tree/RS-Tree.h
        index/R-TreeFamily/RS-Tree/RS-Tree.cpp
        index/GridFamily/RegularGrid/RegularGrid.cpp
        index/GridFamily/RegularGrid/RegularGrid.h
        index/GridFamily/Z-2DGrid/Z-2DGrid.h
        index/GridFamily/Z-2DGrid/Z-2DGrid.cpp
        index/R-TreeFamily/RLR-Tree/RLR-Tree.h
        index/R-TreeFamily/RLR-Tree/RLR-Tree.cpp)

target_link_libraries(PCPS "${TORCH_LIBRARIES}")