cmake_minimum_required(VERSION 3.27)
project(R_BVH_TMP)

set(CMAKE_CXX_STANDARD 20)

add_executable(R_BVH_TMP main.cpp
        util/UTIL.h
        util/UTIL.cpp
        index/Geometry.h
        index/Geometry.cpp
        index/R-BVH.h
        index/R-BVH.cpp
        core/PCPS.h
        core/PCPS.cpp)
