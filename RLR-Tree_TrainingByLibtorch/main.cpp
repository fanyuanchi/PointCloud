#include "index/RLR-Tree-Insert/InsertLearner.h"
#include "index/RLR-Tree-Split/SplitLearner.h"
#include "test/TestForInsert.h"

int DIM = 3;
int seed = 0;

unordered_map<enum TrainType, string> trainNames =
        {
                {TrainType::ChooseSubtree, "Insert"},
                {TrainType::SplitNode, "Split"}
        };

unordered_map<enum Dataset, string> datasetNames =
        {
                {Dataset::Gaussian, "Gaussian"},
                {Dataset::Mountain, "Mountain"},
                {Dataset::RiverBank, "RiverBank"}
        };

void runTraining(bool isTest, bool isP2R, TrainType trainType, Dataset dataset){
    if(isTest){
        DataLoader::rectNum_ = 10000;
    }else{
        DataLoader::rectNum_ = 100000;
    }

    if(trainType == TrainType::ChooseSubtree){
        auto insertLearner = make_unique<InsertLearner>(seed, dataset);
        string basePath = "/home/data/home/fanyuanchi/CLionProjects/RLR-TreeTrain/model/";
        string modeName = datasetNames.find(dataset)->second + "/" + trainNames.find(trainType)->second;
        string modelPath = basePath + modeName;
        if(isP2R){
            modelPath += "P2R.pt";
//            insertLearner->trainP2R(modelPath);
        }else{
            modelPath += "R2R.pt";
            insertLearner->trainR2R(modelPath);
        }
    }else{
        auto splitLearner = make_unique<SplitLearner>(seed, dataset);
        string basePath = "/home/data/home/fanyuanchi/CLionProjects/RLR-TreeTrain/model/";
        string modeName = datasetNames.find(dataset)->second + "/" + trainNames.find(trainType)->second;
        string modelPath = basePath + modeName;
        if(isP2R){
            modelPath += "P2R.pt";
//            splitLearner->trainP2R(modelPath);
        }else{
            modelPath += "R2R.pt";
            splitLearner->trainR2R(modelPath);
        }
    }
}

void runTest(bool isP2R, TrainType trainType, Dataset dataset){
    DataLoader::rectNum_ = 100000;
    if(trainType == TrainType::ChooseSubtree){
        auto insertTest = make_unique<InsertTestTmp>(dataset, isP2R);
        if(isP2R){
            insertTest->testP2R();
        }else{
            insertTest->testR2R();
        }
    }else{
        //TODO
    }
}

int main() {
    torch::manual_seed(seed);
    runTraining(false, false, TrainType::ChooseSubtree, Dataset::Mountain);

//    runTest(false, TrainType::ChooseSubtree, Dataset::Gaussian);
    return 0;
}
