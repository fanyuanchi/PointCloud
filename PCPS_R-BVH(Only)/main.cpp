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
    PCPS::parallelism_ = parallelism;
    PCPS::publishNum_ = publishNum;
    PCPS::registerNum_ = registerNum;
    PCPS::cancelNum_ = cancelNum;
}

void runExperiment(PointSet pointSet, QuerySet querySet, int logIDX, int testType){

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

    if(testType == 0){
        PCPSSystem->runForRBVHRegTest();
    }else if(testType == 1){
        PCPSSystem->runForRBVHPubTest();
    }else if(testType == 2){
        PCPSSystem->runForRBVHCanTest();
    }
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

int main() {
    setRBVHParameter(1000, 0.001, 0.005, 8, 9, 0.1, 10, 0.75);

    runExperiment(PointSet::RiverBank, QuerySet::Large, 1, 1);
    return 0;
}
