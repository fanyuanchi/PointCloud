cmake_minimum_required(VERSION 3.27)
project(RLR_TreeTrain)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/util/libtorch")
find_package(Torch REQUIRED)

add_executable(RLR_TreeTrain main.cpp
        index/Geometry.h
        index/R-Tree.h
        index/R-Tree.cpp
        index/RLR-Tree-Insert/InsertLearner.h
        index/RLR-Tree-Insert/InsertLearner.cpp
        dataLoader/DataLoader.h
        dataLoader/DataLoader.cpp
        test/TestForInsert.h
        test/TestForInsert.cpp
        util/UTIL.h
        util/UTIL.cpp
        index/RLR-Tree-Split/SplitLearner.h
        index/RLR-Tree-Split/SplitLearner.cpp
        index/NeuralNet.h
        index/NeuralNet.cpp)



target_link_libraries(RLR_TreeTrain "${TORCH_LIBRARIES}")