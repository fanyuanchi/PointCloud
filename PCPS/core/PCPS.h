#ifndef PCPS_PCPS_H
#define PCPS_PCPS_H

#include "Worker.h"

enum Stage{Start, Stable, Final, Done};

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
    vector<std::unique_ptr<Worker>> workers_;

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

    int curQueryIDX_ = 0, curPointIDX_ = 0;
    int remainingRegister_ = 0, remainingPublish_ = 0, remainingCancel_ = 0;
    Stage curStage_ = Stage::Start;
    bool stageInitialized_ = false;
    std::mt19937 rng_;

    PCPS(const vector<double>& LOW, const vector<double>& TOP, int seed):
            workers_(PCPS::parallelism_), rng_(seed){
        assert(LOW.size() == DIM && TOP.size() == DIM);
        low_.assign(LOW.begin(), LOW.end());
        top_.assign(TOP.begin(), TOP.end());
    }

    ~PCPS() = default;

    void resetWorker(enum IndexType indexType, enum TestType testType){
        if(workers_.empty()){
            workers_.resize(PCPS::parallelism_);
        }
        for (int idx = 0; idx < PCPS::parallelism_; ++idx) {
            workers_[idx] = std::make_unique<Worker>(idx, low_, top_, indexType, testType);
        }
    }

    void resetWorkerThread(enum TestType testType){
        if(workers_.empty()){
            throw std::runtime_error("no worker can be set");
        }
        for (int idx = 0; idx < PCPS::parallelism_; ++idx) {
            workers_[idx]->index_->resetResult();
            workers_[idx]->resetThread(testType);
        }
    }

    void loadPoint(const string& pointPath);
    void generateCRQuery(const QueryGeneration& qGen);

    Request generateNextRequestForThroughput();
    Request generateNextRequestForBaseline();

    Request generateNextRequestForPublishOnly();
    Request generateNextRequestForRBVHRegistration();
    Request generateNextRequestForRBVHCancel();


    void runForThroughputTest(IndexType indexType);
    void runForBaselineTest(IndexType indexType);

    void runForRBVHRegTest();
    void runForRBVHPubTest();
    void runForRBVHCanTest();
};

#endif //PCPS_PCPS_H
