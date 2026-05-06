#include "./core/PCPS.h"

int DIM = 3;
string pointPath;
string loggerPath = "/home/data/home/fanyuanchi/CLionProjects/PCPS/logger/";
vector<double> LOW;
vector<double> TOP;
const int seed = 524287;
QueryGeneration queryGen;

enum PointSet{RiverBank, Mountain, Paris};
enum QuerySet{Large, Medium, Small, VerySmall};

const unordered_map<enum PointSet, string> pointSetNames =
        {
                {PointSet::RiverBank, "RiverBank"},
                {PointSet::Mountain, "Mountain"},
                {PointSet::Paris, "Paris"}
        };

const unordered_map<enum QuerySet, string> querySetNames =
        {
                {QuerySet::Large, "Large"},
                {QuerySet::Medium, "Medium"},
                {QuerySet::Small, "Small"},
                {QuerySet::VerySmall, "VerySmall"}
        };

const unordered_map<enum QuerySet, double> selectivity =
        {
                {QuerySet::Large, 1e-3},
                {QuerySet::Medium, 1e-4},
                {QuerySet::Small, 1e-5},
                {QuerySet::VerySmall, 1e-6}
        };

void setPointSet(PointSet pointSet){
    if(pointSet == PointSet::RiverBank){
        DIM = 3;
        pointPath = "/home/data/fanyuanchi/PointCloud/RiverBank.csv";
        LOW = {-340, 60, -9.5};
        TOP = {-175, 220, 6.5};
    }else if(pointSet == PointSet::Mountain){
        DIM = 3;
        pointPath = "/home/data/fanyuanchi/PointCloud/Mountain.csv";
        LOW = {-60  , -73.5, -2};
        TOP = { 75.5,  77.5,  48.5};
    }else{
        //TODO: new dataset meta information coming soon...
    }
}

void setQuerySet(int sampleNum, QuerySet queryset,
                 double ratioForSample = 0.6, double centerPerturbation = 1e-4, int windowIntensity = 3){
    queryGen = QueryGeneration(sampleNum, selectivity.find(queryset)->second,
                               ratioForSample, centerPerturbation, windowIntensity);
}

void setPCPSParameter(int parallelism, int publishNum, int registerNum, int cancelNum,
                      double totalMemory){
    Worker::maxM_ = totalMemory / parallelism;
    PCPS::parallelism_ = parallelism;
    PCPS::publishNum_ = publishNum;
    PCPS::registerNum_ = registerNum;
    PCPS::cancelNum_ = cancelNum;
}

void runExperiment(PointSet pointSet, QuerySet querySet, IndexType indexType, TestType testType, int logIDX){
    if(indexType == IndexType::RBvh){
        if(testType == TestType::Baseline){
            cout << "current test type: " << testNames.find(testType)->second << endl;
            throw std::runtime_error("invalid test type for R-BVH");
        }
    }else if(testType != TestType::Baseline && testType != TestType::Throughput){
        cout << "current index type: " << indexNames.find(indexType)->second << endl;
        throw std::runtime_error("invalid index type for " + testNames.find(testType)->second);
    }

    setPointSet(pointSet);
    if(logIDX == 0){
        if(pointSet == PointSet::RiverBank){
            setQuerySet(10000, querySet, 0.6, 1e-4, 3);
            setPCPSParameter(32, 100000, 50000, 10000, 30.0);
        }else if(pointSet == PointSet::Mountain){
            setQuerySet(10000, querySet, 0.2, 1e-4, 3);
            setPCPSParameter(32, 100000, 50000, 10000, 30.0);
        }else{
            //TODO coming soon...
        }
    }else{
        if(pointSet == PointSet::RiverBank){
            setQuerySet(1000000, querySet, 0.6, 1e-4, 3);
            setPCPSParameter(32, 10000000, 5000000, 1000000, 30.0);
        }else if(pointSet == PointSet::Mountain){
            setQuerySet(1000000, querySet, 0.2, 1e-4, 3);
            setPCPSParameter(32, 10000000, 5000000, 1000000, 30.0);
        } else{
            //TODO coming soon...
        }

    }

    auto PCPSSystem = make_unique<PCPS>(LOW, TOP, seed);
    PCPSSystem->loadPoint(pointPath);
    PCPSSystem->generateCRQuery(queryGen);

    string logPath = loggerPath;
    if(logIDX == 0){
        logPath += "Test/test_" + indexNames.find(indexType)->second + "_" + testNames.find(testType)->second
                   + "_" + pointSetNames.find(pointSet)->second + "_" + querySetNames.find(querySet)->second
                   + ".txt";
    }else{
        logPath += indexNames.find(indexType)->second + "/" + testNames.find(testType)->second
                   + "/" + pointSetNames.find(pointSet)->second + "/" + querySetNames.find(querySet)->second
                   + "_" + to_string(logIDX) + ".txt";
    }
    std::filesystem::path fsPath(logPath);
    std::filesystem::create_directories(fsPath.parent_path());
    Logger::instance().init(logPath, true);
    // ===== META =====
    Logger::instance().writeMeta("Experiment", testNames.find(testType)->second);
    Logger::instance().writeMeta("Point set", pointSetNames.find(pointSet)->second);
    Logger::instance().writeMeta("Query set", querySetNames.find(querySet)->second);
    Logger::instance().writeMeta("Index", indexNames.find(indexType)->second);
    Logger::instance().writeMeta("Register counter", PCPS::registerNum_);
    Logger::instance().writeMeta("Publish counter", PCPS::publishNum_);
    Logger::instance().writeMeta("Cancel counter", PCPS::cancelNum_);
    Logger::instance().writeMeta("Thread counter", PCPS::parallelism_);
    Logger::instance().writeMeta("Memory constraint for each worker", Worker::maxM_);
    Logger::instance().writeMeta("Seed", seed);
    Logger::instance().sealMeta();

    if(indexType == IndexType::RBvh){
        if(testType == TestType::Throughput){
            PCPSSystem->runForThroughputTest(IndexType::RBvh);
        }else if(testType == TestType::RegTest){
            PCPSSystem->runForRBVHRegTest();
        }else if(testType == TestType::PubTest){
            PCPSSystem->runForRBVHPubTest();
        }else if(testType == TestType::CanTest){
            PCPSSystem->runForRBVHCanTest();
        }
    }else{
        if(indexType == IndexType::RLRTree){
            if(pointSet == PointSet::RiverBank){
                RLRTree::insertModelPath_ =
                        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/RiverBank/insertModel.pt";
                RLRTree::splitModelPath_ =
                        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/RiverBank/splitModel.pt";
            }else if(pointSet == PointSet::Mountain){
                RLRTree::insertModelPath_ =
                        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/Mountain/insertModel.pt";
                RLRTree::splitModelPath_ =
                        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/Mountain/splitModel.pt";
            }else{
                //TODO coming soon...
            }
        }

        if(testType == TestType::Baseline){
            PCPSSystem->runForBaselineTest(indexType);
        }else if(testType == TestType::Throughput){
            PCPSSystem->runForThroughputTest(indexType);
        }
    }

    Logger::instance().close();
}

void setRBVHParameter(int maxB, double minR, double maxR, int minH, int maxH, double lambda, double kappa, double tau){
    // the capacity of buffer in TreeNode
    RBVHNode::maxB_ = maxB;
    // the minimum and maximum registration limit of a leaf node in R-BVH before split
    RBVH::minR_ = minR; RBVH::maxR_ = maxR;
    // the minimum and maximum height limit of R-BVH
    RBVH::minH_ = minH; RBVH::maxH_ = maxH;
    RBVH::lambda_ = lambda;
    RBVH::kappa_ = kappa;
    // the similarity threshold to detect the distribution of points between two condense of R-BVH
    RBVH::tau_ = tau;
}

void setRTreeParameter(int minEntry, int maxEntry, SplitType splitType){
    RTreeNode::minEntry_ = minEntry;
    RTreeNode::maxEntry_ = maxEntry;
    RTree::splitType_ = splitType;
}

void setRSTreeParameter(int minEntry, int maxEntry, double ratioForReinsert){
    RTreeNode::minEntry_ = minEntry;
    RTreeNode::maxEntry_ = maxEntry;
    RSTree::ratioForReinsert_ = ratioForReinsert;
}

void setRRSTreeParameter(int minEntry, int maxEntry, double RRs = 0.5){
    RTreeNode::minEntry_ = minEntry;
    RTreeNode::maxEntry_ = maxEntry;
    RRSTree::RRs_ = RRs;
}

void setRLRTreeParameter(int minEntry, int maxEntry, int actionSpace = 2){
    RTreeNode::minEntry_ = minEntry;
    RTreeNode::maxEntry_ = maxEntry;
    RLRTree::actionSpace_ = actionSpace;
}

void setRegularGridParameter(int gMin, int gMax, double ratioForDelete){
    RegularGrid::gMin_ = gMin;
    RegularGrid::gMax_ = gMax;
    RegularGrid::ratioForDelete_ = ratioForDelete;
}

void setZ2DGridParameter(int gMin, int gMax, int hMax, int rMin, double ratioForDelete, double ratioForSplit){
    Grid2D::gMin_ = gMin;
    Grid2D::gMax_ = gMax;
    Z2DGrid::hMax_ = hMax;
    Z2DGrid::rMin_ = rMin;
    Z2DGrid::ratioForDelete_ = ratioForDelete;
    Z2DGrid::ratioForSplit_ = ratioForSplit;
}


int main(){
    setRBVHParameter(1000, 0.0005, 0.0009, 8, 9, 0.1, 10, 0.75);
    setRTreeParameter(20, 50, SplitType::Greene);
    setRSTreeParameter(20, 50, 0.3);
    setRRSTreeParameter(20, 50, 0.5);
    setRLRTreeParameter(20, 50, 2);
    setRegularGridParameter(80, 160, 0.05);
    setZ2DGridParameter(100, 300, 10, 64, 0.05, 0.01);

    runExperiment(PointSet::RiverBank, QuerySet::Medium, IndexType::RBvh, TestType::Throughput,
                  3);
    return 0;
}

