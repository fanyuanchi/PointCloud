#ifndef R_BVH_TMP_PCPS_H
#define R_BVH_TMP_PCPS_H

#include "../index/R-BVH.h"
#include "../util/UTIL.h"

struct QueryGeneration{
    int sampleNum_ = 0;
    double ratioForSample_ = 0.6;
    double minSelectivity_ = 0;
    double maxSelectivity_ = 0;
    double centerPerturbation_ = 1e-4;
    int windowIntensity_ = 3;
    QueryGeneration() = default;
    QueryGeneration(int sNum, double selectivity, double rSample = 0.75, double cPerturbation = 1e-4, int wIntensity = 3){
        sampleNum_ = sNum;
        ratioForSample_ = rSample;
        minSelectivity_ = selectivity * 0.9;
        maxSelectivity_ = selectivity * 1.1;
        centerPerturbation_ = cPerturbation;
        windowIntensity_ = wIntensity;
    }
    QueryGeneration(const QueryGeneration& queryGeneration){
        sampleNum_ = queryGeneration.sampleNum_;
        ratioForSample_ = queryGeneration.ratioForSample_;
        minSelectivity_ = queryGeneration.minSelectivity_;
        maxSelectivity_ = queryGeneration.maxSelectivity_;
        centerPerturbation_ = queryGeneration.centerPerturbation_;
        windowIntensity_ = queryGeneration.windowIntensity_;
    }
};

class PCPS{
public:
    vector<Point*> points_{};
    vector<CRQuery*> queries_{};
    vector<int> queryAlive_;
    int curQueryIDX_ = 0;

    vector<std::unique_ptr<RBVH>> indexes_;
    vector<double> timer_;

    vector<double> low_{}, top_{};
    static int parallelism_;
    static int publishNum_;
    static int registerNum_;
    static int cancelNum_;
    // mark the ratio of each kind of request in different stage
    /**
     *  mark that the three stages in workload for testing system throughput,
     *  Stage Start: register requests only
     *  Stage Stable: register, cancel, publish requests, note that the cancel requests only target on random registered queries
     *  Stage Final: cancel requests and publish requests only
     * **/
    // 1) ratioForPublishInStableStage: the ratio of publish request in the stable stage
    // 2) ratioForRegisterInStartStage: the ratio of register request in the start stage
    // 3) ratioForCancelInStableStage:  the ratio of cancel request in the stable stage
    static double ratioForPublishInStableStage_;
    static double ratioForRegisterInStartStage_;
    static double ratioForCancelInStableStage_;
    // mark the ratio of queries generated from sampled point from point cloud dataset

    std::mt19937 rng_;

    PCPS(const vector<double>& LOW, const vector<double>& TOP, int seed):
    indexes_(PCPS::parallelism_), timer_(PCPS::parallelism_, 0.0), rng_(seed){
        assert(LOW.size() == DIM && TOP.size() == DIM);
        low_.assign(LOW.begin(), LOW.end());
        top_.assign(TOP.begin(), TOP.end());
    }

    ~PCPS() = default;

    void resetIndex(){
        if(indexes_.empty()){
            indexes_.resize(PCPS::parallelism_);
        }
        for (int idx = 0; idx < PCPS::parallelism_; ++idx) {
            indexes_[idx] = std::make_unique<RBVH>(low_, top_);
        }
    }

    void loadPoint(const string& pointPath);
    void generateCRQuery(const QueryGeneration& qGen);

    void runForRBVHRegTest();
    void runForRBVHPubTest();
    void runForRBVHCanTest();

    void regQuery(int idx);
    void pubPoint(int idx);
    void incrementalConstruct(int idx);
    void emptyPub(int idx);
    void checkStructure(int idx);
};

#endif //R_BVH_TMP_PCPS_H
