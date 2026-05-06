#include "PCPS.h"

int PCPS::parallelism_ = 32;
int PCPS::publishNum_ = 10000000;
int PCPS::registerNum_ = 5000000;
int PCPS::cancelNum_ = 1000000;
double PCPS::ratioForPublishInStableStage_ = 0.8;
double PCPS::ratioForRegisterInStartStage_ = 0.7;
double PCPS::ratioForCancelInStableStage_ = 0.5;

void PCPS::loadPoint(const string& pointPath){
    points_.resize(PCPS::publishNum_);
    ifstream file(pointPath);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + pointPath);
    }

    string line;
    cout << endl << "Data size: " << PCPS::publishNum_ / 1000 << "k."<< endl;

    for (int idx = 0; idx < PCPS::publishNum_; ++idx) {
        if (!getline(file, line)) {
            throw std::runtime_error("Unexpected EOF at index " + std::to_string(idx));
        }

        stringstream ss(line);
        string attribute;
        vector<double> cord(DIM);
        int dim = 0;

        while (dim < DIM && getline(ss, attribute, ',')) {
            cord[dim++] = std::stod(attribute);
        }

        if (dim != DIM) {
            throw std::runtime_error("Invalid point format at index " + std::to_string(idx));
        }

        for (int d = 0; d < DIM; ++d) {
            if (cord[d] < low_[d] || cord[d] > top_[d]) {
                throw std::runtime_error("Point out of bound at index " + std::to_string(idx));
            }
        }
        points_[idx] = new Point();
        points_[idx]->setCord(cord);
        points_[idx]->getMortonCode(low_, top_);

        if ((idx + 1) % 2500000 == 0) cout << (idx + 1) / 100000 << "00k points loaded." << endl;
    }
    cout << "Data loading completed." << endl << endl;
    file.close();
}

void generateOneSampledCRQuery(const Point &basePoint, CRQuery &out, const vector<double>& LOW, const vector<double>& TOP,
                               double sMin, double sMax, double centerPerturbation, std::mt19937 &rng){
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    // ---------- Step1: perturb center ----------
    double center[DIM];
    for (int dim = 0; dim < DIM; ++dim) {
        double delta = (uni01(rng) * 2.0 - 1.0) * centerPerturbation * (TOP[dim] - LOW[dim]);
        center[dim] = basePoint.cord_[dim] + delta;
    }
    // ---------- Step2: sample selectivity (log-uniform) ----------
    double logSMin = std::log(sMin);
    double logSMax = std::log(sMax);
    double logS = logSMin + uni01(rng) * (logSMax - logSMin);
    double s = std::exp(logS);  // volume ratio

    // ---------- Step3: split volume into 3D extents ----------
    double logAspect[DIM];
    double sum = 0.0;
    for (double & d : logAspect) {
        d = std::log(0.5) + uni01(rng) * std::log(2.0);
        sum += d;
    }
    double extent[DIM];
    for (int dim = 0; dim < DIM; ++dim) {
        double ratio = std::exp(logAspect[dim] - sum / DIM);
        extent[dim] = (TOP[dim] - LOW[dim]) * std::cbrt(s) * ratio;
    }
    // ---------- Step4: compute low / top ----------
    for (int dim = 0; dim < DIM; ++dim) {
        out.low_[dim] = center[dim] - extent[dim] * 0.5;
        out.top_[dim] = center[dim] + extent[dim] * 0.5;
    }
}

void generateOneUniformCRQuery(CRQuery &out, const vector<double>& LOW, const vector<double>& TOP,
                               double sMin, double sMax, std::mt19937 &rng){
    // ---------- Step1: perturb center ----------
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    double center[DIM];
    for(int dim = 0; dim < DIM; ++dim){
        double r = uni01(rng) * 0.8 + 0.1;
        center[dim] = LOW[dim] + r * (TOP[dim] - LOW[dim]);
    }
    // ---------- Step2: sample selectivity (log-uniform) ----------
    double logSMin = std::log(sMin);
    double logSMax = std::log(sMax);
    double logS = logSMin + uni01(rng) * (logSMax - logSMin);
    double s = std::exp(logS);  // volume ratio

    // ---------- Step3: split volume into 3D extents ----------
    double logAspect[DIM];
    double sum = 0.0;
    for (double & d : logAspect) {
        d = std::log(0.5) + uni01(rng) * std::log(2.0);
        sum += d;
    }
    double extent[DIM];
    for (int dim = 0; dim < DIM; ++dim) {
        double ratio = std::exp(logAspect[dim] - sum / DIM);
        extent[dim] = (TOP[dim] - LOW[dim]) * std::cbrt(s) * ratio;
    }
    // ---------- Step4: compute low / top ----------
    for (int dim = 0; dim < DIM; ++dim) {
        out.low_[dim] = center[dim] - extent[dim] * 0.5;
        out.top_[dim] = center[dim] + extent[dim] * 0.5;
    }
}

void PCPS::generateCRQuery(const QueryGeneration& qGen){
    cout << "Query size: " << PCPS::registerNum_ / 1000 << "k."<< endl;
    queries_.resize(PCPS::registerNum_);
    // ---------- Step 1: generate the sample indexes from dataset ----------
    int sampleNum = qGen.sampleNum_;
    int sampledQueryNum = static_cast<int>(PCPS::registerNum_ * qGen.ratioForSample_);
    int uniformQueryNum = PCPS::registerNum_ - sampledQueryNum;
    assert(sampleNum > 0 && sampledQueryNum % sampleNum == 0);
    int queryPerSample =  sampledQueryNum / sampleNum;
    int sampleStart  = static_cast<int>(sampleNum * PCPS::ratioForRegisterInStartStage_);
    int sampledQueryStart = sampleStart * queryPerSample, sampledQueryStable = sampledQueryNum - sampledQueryStart;
    int uniformQueryStart = static_cast<int>(uniformQueryNum * PCPS::ratioForRegisterInStartStage_);
    int uniformQueryStable = uniformQueryNum - uniformQueryStart;

    vector<int> samplePointIndices(PCPS::publishNum_);
    std::iota(samplePointIndices.begin(), samplePointIndices.end(), 0);
    std::shuffle(samplePointIndices.begin(), samplePointIndices.end(), rng_);
    samplePointIndices.resize(sampleNum);
    sort(samplePointIndices.begin()+sampleStart, samplePointIndices.end());

    // ---------- Step 2: generate the queries in v1, v2 and v3 ----------
    // 1) v1 (sampledQueryStartVec):
    // completely shuffled queries (preregistered in stage Start) generated from completely shuffled sampled points
    // 2) v2 (sampledQueryStableVec):
    // sequential ambiguously shuffled queries (registered in stage Stable) generated fromm sampled points with the same dataset sequence
    // 3) v3 (uniformQueryVec):
    // queries with centers uniformly distributed in the entire dataset space with 10% margin in both side along each dimension
    vector<CRQuery> sampledQueryStartVec, sampledQueryStableVec, uniformQueryVec;
    curQueryIDX_ = 0;

    // generate sampled CR queries for stage Start in v1
    for(int idx = 0; idx < sampleStart; ++idx){
        const Point &basePoint = *points_[idx];
        for (int k = 0; k < queryPerSample; ++k) {
            CRQuery q;
            generateOneSampledCRQuery(basePoint,q, low_, top_,qGen.minSelectivity_, qGen.maxSelectivity_,
                                      qGen.centerPerturbation_, rng_);
            sampledQueryStartVec.push_back(q);

            if(++curQueryIDX_ % 2500000 == 0) cout << curQueryIDX_ / 1000 << "k queries generated." << endl;
        }
    }
    std::shuffle(sampledQueryStartVec.begin(), sampledQueryStartVec.end(), rng_);

    // generate sampled CR queries for stage Stable in v2
    for(int idx = sampleStart; idx < sampleNum; ++idx){
        const Point &basePoint = *points_[idx];
        for (int k = 0; k < queryPerSample; ++k) {
            CRQuery q;
            generateOneSampledCRQuery(basePoint,q, low_, top_, qGen.minSelectivity_, qGen.maxSelectivity_,
                                      qGen.centerPerturbation_, rng_);
            sampledQueryStableVec.push_back(q);

            if(++curQueryIDX_ % 2500000 == 0) cout << curQueryIDX_ / 1000 << "k queries generated." << endl;
        }
    }
    int windowSize = qGen.windowIntensity_ * queryPerSample;
    assert(qGen.windowIntensity_ > 1 && sampledQueryStableVec.size() % queryPerSample == 0);
    for (int left = 0; left + windowSize <= sampledQueryStableVec.size(); left += queryPerSample) {
        std::shuffle(sampledQueryStableVec.begin() + left,
                     sampledQueryStableVec.begin() + left + windowSize, rng_);
    }

    // generate uniform CR queries in v3
    for(int idx = 0; idx < uniformQueryNum; ++idx){
        CRQuery q;
        generateOneUniformCRQuery(q, low_, top_, qGen.minSelectivity_, qGen.maxSelectivity_, rng_);
        uniformQueryVec.push_back(q);

        if(++curQueryIDX_ % 2500000 == 0) cout << curQueryIDX_ / 1000 << "k queries generated." << endl;
    }
    assert(curQueryIDX_ == PCPS::registerNum_);

    // ---------- Step 3: mix the queries in v1, v2 and v3 to generate the registration workload for throughput test ----------
    curQueryIDX_ = 0;
    // mix the queries in v1 and v3 in random sequence to generate the registration workload for stage Start
    int remainingSampledQueryStart = sampledQueryStart, remainingSampledQueryStable = sampledQueryStable;
    int remainingUniformQueryStart = uniformQueryStart, remainingUniformQueryStable = uniformQueryStable;
    int v1IDX = 0, v2IDX = 0, v3IDX = 0;
    while(remainingSampledQueryStart + remainingUniformQueryStart > 0){
        int total = remainingSampledQueryStart + remainingUniformQueryStart;
        std::uniform_int_distribution<int> dist(1, total);
        int pick = dist(rng_);
        if(pick <= remainingSampledQueryStart){
            assert(curQueryIDX_ < PCPS::registerNum_ && v1IDX < sampledQueryStart);
            queries_[curQueryIDX_++] = new CRQuery(sampledQueryStartVec[v1IDX++]);
            --remainingSampledQueryStart;
        }else{
            assert(curQueryIDX_ < PCPS::registerNum_ && v3IDX < uniformQueryStart);
            queries_[curQueryIDX_++] = new CRQuery(uniformQueryVec[v3IDX++]);
            --remainingUniformQueryStart;
        }
    }
    assert(v1IDX == sampledQueryStart && v3IDX == uniformQueryStart);
    // mix the queries in v2 and v3 in random sequence to generate the registration workload for stage Stable
    while(remainingSampledQueryStable + remainingUniformQueryStable > 0){
        int total = remainingSampledQueryStable + remainingUniformQueryStable;
        std::uniform_int_distribution<int> dist(1, total);
        int pick = dist(rng_);
        if(pick <= remainingSampledQueryStable){
            assert(curQueryIDX_ < PCPS::registerNum_ && v2IDX < sampledQueryStable);
            queries_[curQueryIDX_++] = new CRQuery(sampledQueryStableVec[v2IDX++]);
            --remainingSampledQueryStable;
        }else{
            assert(curQueryIDX_ < PCPS::registerNum_ && v3IDX < uniformQueryNum);
            queries_[curQueryIDX_++] = new CRQuery(uniformQueryVec[v3IDX++]);
            --remainingUniformQueryStable;
        }
    }

    //generate the MRL for each Query with new seed
    float MPLs[5] = {0.01, 0.02, 0.03, 0.04, 0.05};
    std::mt19937 gen(0);
    std::uniform_int_distribution<int> distIDX(0, 4);
    for(int idx = 0; idx < PCPS::registerNum_; ++idx){
        int MPLIDX = distIDX(gen);
        queries_[idx]->MLP_ = MPLs[MPLIDX];
    }

    assert(curQueryIDX_ == PCPS::registerNum_ && v2IDX == sampledQueryStable && v3IDX == uniformQueryNum);
    cout << "Query generation completed." << endl << endl;
}


/**
 * generateNextRequest for throughput test
 * 1) Stage::Start:  only register requests (Partial)
 * 2) Stage::Stable: register requests (The Rest), publish request (Partial), cancel request (Partial)
 * 3) Stage::Final:  publish request (The Rest), cancel request (The Rest)
 */
Request PCPS::generateNextRequestForThroughput() {
    static bool is_initialized = false;
    if (!is_initialized) {
        stageInitialized_ = false;
        curStage_ = Stage::Start;
        is_initialized = true;
    }
    // ---------- Step 0: initialize the remaining request number ----------
    if (!stageInitialized_) {
        if (curStage_ == Stage::Start) {
            curQueryIDX_ = 0;
            remainingRegister_ = static_cast<int>(PCPS::registerNum_ * PCPS::ratioForRegisterInStartStage_);
            remainingPublish_ = 0;
            remainingCancel_  = 0;
        }else if (curStage_ == Stage::Stable) {
            curPointIDX_ = 0;
            remainingRegister_ = PCPS::registerNum_ - static_cast<int>(PCPS::registerNum_ * PCPS::ratioForRegisterInStartStage_);
            remainingPublish_ = static_cast<int>(PCPS::publishNum_ * PCPS::ratioForPublishInStableStage_);
            remainingCancel_ = static_cast<int>(PCPS::cancelNum_ * PCPS::ratioForCancelInStableStage_);
        }else if (curStage_ == Stage::Final) {
            remainingRegister_ = 0;
            remainingPublish_ = PCPS::publishNum_ - static_cast<int>(PCPS::publishNum_ * PCPS::ratioForPublishInStableStage_);
            remainingCancel_ = PCPS::cancelNum_ - static_cast<int>(PCPS::cancelNum_ * PCPS::ratioForCancelInStableStage_);
        }else { // DONE
            assert(curQueryIDX_ == PCPS::registerNum_ && curPointIDX_ == PCPS::publishNum_);
            is_initialized = false;
            return {RequestType::Stop};
        }
        stageInitialized_ = true;
    }

    // ---------- Step 1: shift the stage and output the runtime information of the last stage ----------
    if (remainingRegister_ + remainingPublish_ + remainingCancel_ == 0) {
        stageInitialized_ = false;
        if (curStage_ == Stage::Start){
            cout << "Stage::Start -> Stage::Stable" << endl;
            curStage_ = Stage::Stable;
        }else if (curStage_ == Stage::Stable){
            cout << "Stage::Stable -> Stage::Final" << endl;
            curStage_ = Stage::Final;
        }else if (curStage_ == Stage::Final){
            cout << "Stage::Final -> Stage::Done" << endl;
            curStage_ = Stage::Done;
        }
        size_t maxReg = 0, minReg = LONG_LONG_MAX, sumReg = 0;
        for(auto& worker: workers_){
            maxReg = max(maxReg, worker->index_->regCounter_);
            minReg = min(minReg, worker->index_->regCounter_);
            sumReg += worker->index_->regCounter_;
        }
        double maxRegDBL = static_cast<double>(maxReg) / 1e6;
        double minRegDBL = static_cast<double>(minReg) / 1e6;
        double sumRegDBL = static_cast<double>(sumReg) / 1e6;
        printf("MaxReg: %.3lfM\n", maxRegDBL);
        printf("MinReg: %.3lfM\n", minRegDBL);
        printf("SumReg: %.3lfM\n", sumRegDBL);
        printf("MinMaxRegRatio: %.3lf\n\n", maxRegDBL / minRegDBL);
        return generateNextRequestForThroughput(); // enter the next stage
    }

    // ---------- Step 2: pick and output a request ----------
    int total = remainingRegister_ + remainingPublish_ + remainingCancel_;
    std::uniform_int_distribution<int> dist(1, total);
    int pick = dist(rng_);
    // ---- REGISTER ----
    if (pick <= remainingRegister_) {
        queryAlive_.push_back(curQueryIDX_);
        --remainingRegister_;
        return {RequestType::Register,queries_[curQueryIDX_++]};
    }
    pick -= remainingRegister_;
    // ---- PUBLISH ----
    if (pick <= remainingPublish_) {
        --remainingPublish_;
        return {points_[curPointIDX_++]};
    }
    // ---- CANCEL ----
    {
        std::uniform_int_distribution<int> itDist(0, queryAlive_.size() - 1);
        int offset = itDist(rng_);
        int queryIDX = queryAlive_[offset];
        queryAlive_[offset] = queryAlive_.back();
        queryAlive_.pop_back();
        --remainingCancel_;
        return {RequestType::Cancel,queries_[queryIDX]};
    }
}

/**
 * generateNextRequest for baselines test
 * 1) Stage::Register: one-by-one process query registration request
 * 2) Stage::Publish:  one-by-one process point publishing request
 * 3) Stage::Cancel:   one-by-one process query cancel request (same interface implemented by various baselines,
 *                     including one-by-one delete and batch delete)
 */
Request PCPS::generateNextRequestForBaseline() {
    static bool is_initialized = false;
    if (!is_initialized) {
        stageInitialized_ = false;
        curStage_ = Stage::Start;
        is_initialized = true;
    }
    // ---------- Step 0: initialize the remaining request number ----------
    if (!stageInitialized_) {
        if (curStage_ == Stage::Start) {
            curQueryIDX_ = 0;
            remainingRegister_ = PCPS::registerNum_;
            remainingPublish_ = 0;
            remainingCancel_ = 0;
        }else if (curStage_ == Stage::Stable) {
            curPointIDX_ = 0;
            remainingRegister_ = 0;
            remainingPublish_ = PCPS::publishNum_;
            remainingCancel_ = 0;
        }else if (curStage_ == Stage::Final) {
            curQueryIDX_ = 0;
            remainingRegister_ = 0;
            remainingPublish_ = 0;
            remainingCancel_ = PCPS::cancelNum_;

            queryAlive_.resize(PCPS::registerNum_);
            std::iota(queryAlive_.begin(), queryAlive_.end(), 0);
            std::shuffle(queryAlive_.begin(), queryAlive_.end(), rng_);
            queryAlive_.resize(PCPS::cancelNum_);
        }else { // DONE
            assert(curQueryIDX_ == PCPS::cancelNum_ && curPointIDX_ == PCPS::publishNum_);
            is_initialized = false;
            return {RequestType::Stop};
        }
        stageInitialized_ = true;
    }
    // ---------- Step 1: shift the stage and output the runtime information of the last stage ----------
    if (remainingRegister_ + remainingPublish_ + remainingCancel_ == 0) {
        stageInitialized_ = false;
        if (curStage_ == Stage::Start){
            cout << "Stage::Register -> Stage::Publish" << endl;
            curStage_ = Stage::Stable;
        }else if (curStage_ == Stage::Stable){
            cout << "Stage::Publish -> Stage::Cancel" << endl;
            curStage_ = Stage::Final;
        }else if (curStage_ == Stage::Final){
            cout << "Stage::Cancel -> Stage::Done" << endl;
            curStage_ = Stage::Done;
        }
        size_t maxReg = 0, minReg = LONG_LONG_MAX, sumReg = 0;
        for(auto& worker: workers_){
            maxReg = max(maxReg, worker->index_->regCounter_);
            minReg = min(minReg, worker->index_->regCounter_);
            sumReg += worker->index_->regCounter_;
        }
        double maxRegDBL = static_cast<double>(maxReg) / 1e6;
        double minRegDBL = static_cast<double>(minReg) / 1e6;
        double sumRegDBL = static_cast<double>(sumReg) / 1e6;
        printf("MaxReg: %.3lfM\n", maxRegDBL);
        printf("MinReg: %.3lfM\n", minRegDBL);
        printf("SumReg: %.3lfM\n", sumRegDBL);
        printf("MinMaxRegRatio: %.3lf\n\n", maxRegDBL / minRegDBL);
        return generateNextRequestForBaseline(); // enter the next stage
    }
    // ---------- Step 2: output a request according to the current stage since there's only one request type in each stage ----------
    // ---- REGISTER ----
    if(curStage_ == Stage::Start){
        --remainingRegister_;
        return {RequestType::Register, queries_[curQueryIDX_++]};
    }
    // ---- PUBLISH ----
    if(curStage_ == Stage::Stable){
        --remainingPublish_;
        return {points_[curPointIDX_++]};
    }
    // ---- CANCEL ----
    {
        --remainingCancel_;
        int queryIDX = queryAlive_[curQueryIDX_++];
        return {RequestType::Cancel, queries_[queryIDX]};
    }
}

/**
 * generateNextRequest for point publishing requests reception (do not process) test
 *                     and R-BVH point publishing requests processing test
 *                     and R-BVH query cancel requests processing test (controlled group)
 */
Request PCPS::generateNextRequestForPublishOnly(){
//    static double startTime = thread_real_time(), endTime;
    static bool is_initialized = false;
    if (!is_initialized) {
        curPointIDX_ = 0;
        is_initialized = true;
    }
    if(curPointIDX_ < PCPS::publishNum_){
        return {points_[curPointIDX_++]};
    }
    return {RequestType::Stop};
}

/**
 * generateNextRequest for R-BVH static query registration requests processing test
 *                     and R-BVH point publishing requests processing test
 *                     and R-BVH pre-build in query cancel requests processing test
 *    Stages:
 * 1) Stage::PreRegister (All):        pre-register all queries
 * 2) Stage::Incremental Construction: make publishing requests processing trigger only incremental R-BVH construction
 *    Notes:
 * 1) in R-BVH static query registration requests processing test, matching operations are denied,
 *    (No "publishFull" or "publishPart" called in function "incrementalConstruction")
 * 2) in R-BVH point publishing requests processing test, all operations are activated
 * 3) in R-BVH query cancel requests processing test, point reception of queries and index structure update operation are denied,
 *    (No "splitNode" called in function "publishSynchronizedDelete")
 */
Request PCPS::generateNextRequestForRBVHRegistration(){
    static bool is_initialized = false;
    if (!is_initialized) {
        stageInitialized_ = false;
        curStage_ = Stage::Start;
        is_initialized = true;
    }
    // ---------- Step 0: initialize the remaining request number ----------
    if (!stageInitialized_) {
        if (curStage_ == Stage::Start) {
            curQueryIDX_ = 0;
            remainingRegister_ = PCPS::registerNum_;
            remainingPublish_ = 0;
        }else if (curStage_ == Stage::Stable) {
            curPointIDX_ = 0;
            remainingRegister_ = 0;
            remainingPublish_ = PCPS::publishNum_;
        }else { // DONE
            assert(curQueryIDX_ == PCPS::registerNum_ && curPointIDX_ == PCPS::publishNum_);
            is_initialized = false;
            return {RequestType::Stop};
        }
        stageInitialized_ = true;
    }

    // ---------- Step 1: shift the stage and output the runtime information of the last stage ----------
    if (remainingRegister_ + remainingPublish_ == 0) {
        stageInitialized_ = false;
        if (curStage_ == Stage::Start){
            cout << "Stage::PreRegister (All) -> Stage::Incremental Construction" << endl;
            curStage_ = Stage::Stable;
        }else if (curStage_ == Stage::Stable){
            cout << "Stage::Incremental Construction -> Stage::Done" << endl;
            curStage_ = Stage::Done;
        }
        size_t maxReg = 0, minReg = LONG_LONG_MAX, sumReg = 0;
        for(auto& worker: workers_){
            maxReg = max(maxReg, worker->index_->regCounter_);
            minReg = min(minReg, worker->index_->regCounter_);
            sumReg += worker->index_->regCounter_;
        }
        double maxRegDBL = static_cast<double>(maxReg) / 1e6;
        double minRegDBL = static_cast<double>(minReg) / 1e6;
        double sumRegDBL = static_cast<double>(sumReg) / 1e6;
        printf("MaxReg: %.3lfM\n", maxRegDBL);
        printf("MinReg: %.3lfM\n", minRegDBL);
        printf("SumReg: %.3lfM\n", sumRegDBL);
        printf("MinMaxRegRatio: %.3lf\n\n", maxRegDBL / minRegDBL);
        return generateNextRequestForRBVHRegistration(); // enter the next stage
    }

    // ---------- Step 2: output a request according to the current stage since there's only one request type in each stage ----------
    // ---- REGISTER ----
    if(curStage_ == Stage::Start){
        --remainingRegister_;
        return {RequestType::Register, queries_[curQueryIDX_++]};
    }
    // ---- PUBLISH ----
    {
        --remainingPublish_;
        return {points_[curPointIDX_++]};
    }
}

/**
 * generateNextRequest for R-BVH query cancel requests processing test
 * 1) Stage::Lazy Mark (All)            : mark all canceled queries in R-BVH
 * 2) Stage::Publish Synchronized Delete: make publishing requests processing trigger only synchronized query delete
 */
Request PCPS::generateNextRequestForRBVHCancel(){
    static bool is_initialized = false;
    if (!is_initialized) {
        stageInitialized_ = false;
        curStage_ = Stage::Start;
        is_initialized = true;
    }
    // ---------- Step 0: initialize the remaining request number ----------
    if (!stageInitialized_) {
        if (curStage_ == Stage::Start) {
            curQueryIDX_ = 0;
            remainingPublish_ = 0;
            remainingCancel_ = PCPS::cancelNum_;

            queryAlive_.resize(PCPS::registerNum_);
            std::iota(queryAlive_.begin(), queryAlive_.end(), 0);
            std::shuffle(queryAlive_.begin(), queryAlive_.end(), rng_);
            queryAlive_.resize(PCPS::cancelNum_);
        }else if (curStage_ == Stage::Stable) {
            curPointIDX_ = 0;
            remainingPublish_ = PCPS::publishNum_;
            remainingCancel_ = 0;
        }else { // DONE
            assert(curQueryIDX_ == PCPS::cancelNum_ && curPointIDX_ == PCPS::publishNum_);
            is_initialized = false;
            return {RequestType::Stop};
        }
        stageInitialized_ = true;
    }

    // ---------- Step 1: shift the stage and output the runtime information of the last stage ----------
    if (remainingPublish_ + remainingCancel_ == 0) {
        stageInitialized_ = false;
        if (curStage_ == Stage::Start){
            cout << "Stage::Lazy Mark (All) -> Stage::Publish Synchronized Delete" << endl;
            curStage_ = Stage::Stable;
        }else if (curStage_ == Stage::Stable){
            cout << "Stage::Publish Synchronized Delete -> Stage::Done" << endl;
            curStage_ = Stage::Done;
        }
        size_t maxReg = 0, minReg = LONG_LONG_MAX, sumReg = 0;
        for(auto& worker: workers_){
            maxReg = max(maxReg, worker->index_->regCounter_);
            minReg = min(minReg, worker->index_->regCounter_);
            sumReg += worker->index_->regCounter_;
        }
        double maxRegDBL = static_cast<double>(maxReg) / 1e6;
        double minRegDBL = static_cast<double>(minReg) / 1e6;
        double sumRegDBL = static_cast<double>(sumReg) / 1e6;
        printf("MaxReg: %.3lfM\n", maxRegDBL);
        printf("MinReg: %.3lfM\n", minRegDBL);
        printf("SumReg: %.3lfM\n", sumRegDBL);
        printf("MinMaxRegRatio: %.3lf\n\n", maxRegDBL / minRegDBL);
        return generateNextRequestForRBVHCancel(); // enter the next stage
    }

    // ---------- Step 2: output a request according to the current stage since there's only one request type in each stage ----------
    // ---- CANCEL ----
    if(curStage_ == Stage::Start){
        --remainingCancel_;
        int queryIDX = queryAlive_[curQueryIDX_++];
        return {RequestType::Cancel, queries_[queryIDX]};
    }
    // ---- PUBLISH ----
    {
        --remainingPublish_;
        return {points_[curPointIDX_++]};
    }
}

void PCPS::runForThroughputTest(IndexType indexType){
    resetWorker(indexType, TestType::Throughput);
    int curQueryIDX = 0;
    while (true) {
        Request request = generateNextRequestForThroughput();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            Result result;
            double totalMemory = 0.0;
            double peakMemory = 0.0;
            for(auto& worker: workers_){
                for(int idx = 0; idx < 9; ++idx){
                    result.resList_[idx] += worker->result_.resList_[idx];
                }
                totalMemory += worker->memory_;
                peakMemory += worker->peak_;
                printf("Worker %-2d", worker->workerID_ );
                printf(" memory usage: %.3lf / %.3lf GB\n", worker->memory_, worker->peak_);
                result.total_ += worker->result_.total_;
            }

            cout << "IN THROUGHPUT TEST: " << endl;
            result.printResult();
            printf("AVERAGED CURRENT MEMORY: %.3lf GB\n", totalMemory / PCPS::parallelism_);
            printf("AVERAGED PEAK MEMORY   : %.3lf GB\n", peakMemory / PCPS::parallelism_);
            cout << endl;
            break;
        }else if(request.type_ == RequestType::Register){
            int wid = curQueryIDX++ % PCPS::parallelism_;
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            Request emptyReq = {RequestType::None}; // do flush in R-BVH, do nothing in other indexes
            workers_[wid]->submitTask(request);
        }else if(request.type_ == RequestType::Cancel){
            int wid = std::get<CRQuery*>(request.payload_)->indexID_;
            workers_[wid]->submitTask(request);
        }else if(request.type_ == RequestType::Publish){
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    double totalTime = 0.0, averageTime = 0.0;
    for (auto& worker : workers_){
        totalTime = max(totalTime, worker->runTime_);
        averageTime += worker->runTime_ / PCPS::parallelism_;
    }
    double totalRequestNum = PCPS::registerNum_ + PCPS::publishNum_ + PCPS::cancelNum_;
    double averageThroughput = totalRequestNum / 1e3 / averageTime;
    double totalThroughput = totalRequestNum / 1e3 / totalTime;
    cout << endl << endl;
    printf("PC-PS RUN %.3lf ", totalTime);
    cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
    cout << "THROUGHPUT TEST COMPLETE" << endl;
    cout << "INDEX TYPE: " << indexNames.find(workers_[0]->indexType_)->second << endl;
    printf("TOTAL TIME:          : %.3lf / %.3lf SECS\n", averageTime, totalTime);
    printf("PC-PS THROUGHPUT:    : %.3lf / %.3lfK REQUESTS PER SEC\n", averageThroughput, totalThroughput);
    cout << endl << endl;

    Logger::instance().writeResult("Average time (seconds)", averageTime);
    Logger::instance().writeResult("Total time (seconds)", totalTime);
    Logger::instance().writeResult("PC-PS averaged throughput (k requests / second)", averageThroughput);
    Logger::instance().writeResult("PC-PS total throughput (k requests / second)", totalThroughput);
}

void PCPS::runForBaselineTest(IndexType indexType){
    resetWorker(indexType, TestType::Baseline);
    int curQueryIDX = 0;
    while (true) {
        Request request = generateNextRequestForBaseline();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }
            Result result;
            double totalMemory = 0.0;
            double peakMemory = 0.0;
            for(auto& worker: workers_){
                for(int idx = 0; idx < 9; ++idx){
                    result.resList_[idx] += worker->result_.resList_[idx];
                }
                totalMemory += worker->memory_;
                peakMemory += worker->peak_;
                printf("Worker %-2d", worker->workerID_ );
                printf(" memory usage: %.3lf / %.3lf GB\n", worker->memory_, worker->peak_);
                result.total_ += worker->result_.total_;

                printf("Worker %-2d:\n", worker->workerID_ );
                printf("Registration latency: %.3lf\n", worker->stageTime_[0]);
                printf("Publishing latency: %.3lf\n", worker->stageTime_[1]);
                printf("Cancel latency: %.3lf\n\n", worker->stageTime_[2]);
            }

            cout << endl << "IN BASELINE TEST:" << endl;
            result.printResult();
            printf("AVERAGED CURRENT MEMORY: %.3lf GB\n", totalMemory / PCPS::parallelism_);
            printf("AVERAGED PEAK MEMORY   : %.3lf GB\n", peakMemory / PCPS::parallelism_);
            cout << endl;
            break;
        }else if(request.type_ == RequestType::Register){
            int wid = curQueryIDX++ % PCPS::parallelism_;
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            workers_[wid]->submitTask(request);
        }else if(request.type_ == RequestType::Cancel){
            int wid = std::get<CRQuery*>(request.payload_)->indexID_;
            workers_[wid]->submitTask(request);
        }else if(request.type_ == RequestType::Publish){
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    double totalStageTime[3] = {0.0, 0.0, 0.0}, averageStageTime[3] = {0.0, 0.0, 0.0};
    double totalTime = 0.0, averageTime = 0.0;
    for (auto& worker : workers_){
        totalTime = max(totalTime, worker->runTime_);
        averageTime += worker->runTime_ / PCPS::parallelism_;
        for(int stageIDX = 0; stageIDX < 3; ++stageIDX){
            totalStageTime[stageIDX] = max(totalStageTime[stageIDX], worker->stageTime_[stageIDX]);
            averageStageTime[stageIDX] += worker->stageTime_[stageIDX] / PCPS::parallelism_;
        }
    }
    double averagePublishThroughput = averageStageTime[1] / PCPS::publishNum_ * 1e3;
    double totalPublishThroughput = totalStageTime[1] / PCPS::publishNum_ * 1e3;
    cout << endl << endl;
    printf("PC-PS RUN %.3lf / %.3lf", averageTime, totalTime);
    cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
    cout << "BASELINE TEST COMPLETE" << endl;
    cout << "INDEX TYPE: " << indexNames.find(workers_[0]->indexType_)->second << endl;
    printf("TIME FOR QUERY REGISTRATION: %.3lf / %.3lf SECS\n", averageStageTime[0], totalStageTime[0]);
    printf("TIME FOR POINT PUBLISHING  : %.3lf  / %.3lf SECS\n", averageStageTime[1], totalStageTime[1]);
    printf("PUBLISHING THROUGHPUT      : %.3lf / %.3lf SECS PER 1K POINTS\n", averagePublishThroughput, totalPublishThroughput);
    printf("TIME FOR QUERY CANCEL      : %.3lf  / %.3lf SECS\n", averageStageTime[2], totalStageTime[2]);
    cout << endl << endl;

    Logger::instance().writeResult("Averaged time for query registration (seconds)", averageStageTime[0]);
    Logger::instance().writeResult("Averaged time for point publishing (seconds)", averageStageTime[1]);
    Logger::instance().writeResult("Averaged publishing throughput (second / 1k points)", averagePublishThroughput);
    Logger::instance().writeResult("Averaged time for query cancel (seconds)", averageStageTime[2]);
    Logger::instance().writeResult("Averaged total time (seconds)", averageTime);

    Logger::instance().writeResult("Max time for query registration (seconds)", totalStageTime[0]);
    Logger::instance().writeResult("Max time for point publishing (seconds)", totalStageTime[1]);
    Logger::instance().writeResult("Max publishing throughput (second / 1k points)", totalPublishThroughput);
    Logger::instance().writeResult("Max time for query cancel (seconds)", totalStageTime[2]);
    Logger::instance().writeResult("Max total time (seconds)", totalTime);
}

/**
 * The R-BVH Registration Latency Test contains the following three steps:
 * 1) Step 1: Do Registration Request Process Test to acquire the latency of processing registration requests
 * 2) Step 2: Do Non-Matching Publishing Request Process Test to acquire the latency of incremental construction
 * 3) Step 3: Add the two latencies to acquire the latency of R-BVH Registration
 */
void PCPS::runForRBVHRegTest() {
    resetWorker(IndexType::RBvh, TestType::RegTest);
    int curQueryIDX = 0;
    while (true) {
        Request request = generateNextRequestForRBVHRegistration();
        if(request.type_ == RequestType::Stop){
            // ---------- Step 3: Acquire the total temporal duration of pre-registration ----------
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            double totalMemory = 0.0;
            double peakMemory = 0.0;
            for(auto& worker: workers_){
                totalMemory += worker->memory_;
                peakMemory += worker->peak_;
                printf("Worker %-2d", worker->workerID_ );
                printf(" memory usage: %.3lf / %.3lf GB\n", worker->memory_, worker->peak_);
            }

            cout << "IN R-BVH REGISTRATION LATENCY TEST:" << endl;
            printf("AVERAGED CURRENT MEMORY: %.3lf GB\n", totalMemory / PCPS::parallelism_);
            printf("AVERAGED PEAK MEMORY   : %.3lf GB\n", peakMemory / PCPS::parallelism_);
            cout << endl;
            break;
        }else if(request.type_ == RequestType::Register){
            // ---------- Step 1: Acquire the temporal duration of pre-registration requests process ----------
            int wid = curQueryIDX++ % PCPS::parallelism_;
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            size_t minReg = LONG_LONG_MAX;
            for(auto& worker : workers_){
                if(worker->index_->regCounter_ <= minReg){
                    minReg = worker->index_->regCounter_;
                    wid = worker->workerID_;
                }
            }
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            workers_[wid]->submitTask(request);
        }else{
            // ---------- Step 2: Acquire the temporal duration of non-matching requests process ----------
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    double totalRegTime = 0.0, averagedRegTime = 0.0;
    for(auto& worker: workers_){
        totalRegTime = max(totalRegTime, worker->runTime_);
        averagedRegTime += worker->runTime_ / PCPS::parallelism_;
    }

    cout << endl << endl;
    printf("PC-PS RUN %.3lf / %.3lf ", averagedRegTime, totalRegTime);
    cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
    cout << "R-BVH REGISTRATION TEST COMPLETE" << endl;
    cout << "INDEX TYPE: " << indexNames.find(workers_[0]->indexType_)->second << endl;
    cout << endl << endl;

    Logger::instance().writeResult("Averaged time for pre-registration request process (seconds)", averagedRegTime);
    Logger::instance().writeLine("\n");
    Logger::instance().writeResult("Total time for pre-registration request process (seconds)", totalRegTime);
}

/**
 * The R-BVH Publishing Latency Test contains the following three steps:
 * 2) Step 1: Do Pre-Registration and Incremental Construction to prebuild R-BVH the fix the structure during the next step
 * 3) Step 2: Redo Normal Publishing Request Process Test to acquire the net latency
 *            (only publishing request process and matching operation) requests
 */
void PCPS::runForRBVHPubTest(){
    // ---------- Step 1: Acquire the temporal duration of pre-registration requests process ----------
    int curQueryIDX = 0;
    resetWorker(IndexType::RBvh, TestType::RegTest);
    double preRegStartTime = thread_real_time(), preRegEndTime;
    while (true) {
        Request request = generateNextRequestForRBVHRegistration();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            preRegEndTime = thread_real_time();
            cout << "IN R-BVH PUBLISHING LATENCY TEST (PHASE PRE-REGISTER):" << endl << "PC-PS RUN ";
            printf("%.3lf ", preRegEndTime - preRegStartTime);
            cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
            break;
        }else if(request.type_ == RequestType::Register){
            int wid = curQueryIDX++ % PCPS::parallelism_;
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            workers_[wid]->submitTask(request);
        }else{
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    Logger::instance().writePhase("In phase pre-build, PC-PS run time (seconds): ", preRegEndTime - preRegStartTime);

    // ---------- Step 2: Acquire the temporal duration of normal publishing requests process ----------
    resetWorkerThread(TestType::PubTest);
    double pubStartTime = thread_real_time(), pubEndTime;
    while (true) {
        Request request = generateNextRequestForPublishOnly();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            pubEndTime = thread_real_time();
            Result result;
            for(auto& worker: workers_){
                for(int idx = 0; idx < 9; ++idx){
                    result.resList_[idx] += worker->result_.resList_[idx];
                }
                result.total_ += worker->result_.total_;
            }
            cout << "IN R-BVH PUBLISHING LATENCY TEST (PHASE PUBLISH ONLY):" << endl << "PC-PS RUN ";
            printf("%.3lf ", pubEndTime - pubStartTime);
            cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
            result.printResult();
            break;
        }else{
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    Logger::instance().writePhase("In phase registration-publishing, PC-PS run time (seconds): ",
                                  pubEndTime - pubStartTime);

    double preRegTime = preRegEndTime - preRegStartTime;
    double pubTime = pubEndTime - pubStartTime;

    double publishThroughput = pubTime / PCPS::publishNum_ * 1e3;
    cout << endl << endl;
    cout << "R-BVH PUBLISHING TEST COMPLETE" << endl;
    printf("TIME FOR PRE-REGISTRATION REQUEST PROCESS: %.3lf SECS\n", preRegTime);
    printf("TIME FOR PUBLISHING REQUEST PROCESS      : %.3lf SECS\n", pubTime);
    printf("R-BVH PUBLISHING THROUGHPUT              : %.3lf SECS PER 1K POINTS\n", publishThroughput);
    cout << endl << endl;

    Logger::instance().writeResult("Time for pre-registration request process (seconds)", preRegTime);
    Logger::instance().writeResult("Time for publishing request process (seconds)", pubTime);
    Logger::instance().writeResult("R-BVH publishing throughput (second / 1k points)", publishThroughput);
}

/**
 * The R-BVH Cancel Latency Test contains the following three steps:
 * Steps:
 * 1) Step 1: Do Pre-Registration and Incremental Construction to prebuild R-BVH the fix the structure during the next two steps
 * 2) Step 2: Do Query Cancel Request Process Test to acquire the gross latency of processing of lazy-mark marking
 *            and publishing synchronized deletion
 *            with operation "splitNode" denied in function "publishSynchronizedDelete"
 * 3) Step 3: Do Normal Publishing Request Process Test to acquire the gross latency of processing normal publishing requests
 *            with the same publishing traces (same points and same R-BVH structure) and alive queries as in Step 2
 */
void PCPS::runForRBVHCanTest(){
    // ---------- Step 1: pre-register and incrementally construct R-BVH (prebuild and fix) ----------
    int curQueryIDX = 0;
    resetWorker(IndexType::RBvh, TestType::RegTest);
    double buildStartTime = thread_real_time(), buildEndTime;
    while (true) {
        Request request = generateNextRequestForRBVHRegistration();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            buildEndTime = thread_real_time();
            cout << "IN R-BVH CANCEL LATENCY TEST (PHASE PRE-BUILD):" << endl << "PC-PS RUN ";
            printf("%.3lf ", buildEndTime - buildStartTime);
            cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
            break;
        }else if(request.type_ == RequestType::Register){
            int wid = curQueryIDX++ % PCPS::parallelism_;
            std::get<CRQuery*>(request.payload_)->indexID_ = wid;
            workers_[wid]->submitTask(request);
        }else{
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    Logger::instance().writePhase("In phase pre-build, PC-PS run time (seconds): ", buildEndTime - buildStartTime);

    // ---------- Step 2: Acquire the temporal duration of cancel requests process (including publishing requests)-----
    resetWorkerThread(TestType::CanTest);
    double canPubStartTime = thread_real_time(), canPubEndTime;
    while (true) {
        Request request = generateNextRequestForRBVHCancel();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            canPubEndTime = thread_real_time();
            Result result;
            for(auto& worker: workers_){
                for(int idx = 0; idx < 9; ++idx){
                    result.resList_[idx] += worker->result_.resList_[idx];
                }
                result.total_ += worker->result_.total_;
            }
            cout << "IN R-BVH CANCEL LATENCY TEST (PHASE EXP-GROUP): " << endl;
            printf("PC-PS RUN %.3lf ", canPubEndTime - canPubStartTime);
            cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
            result.printResult();
            break;
        }else if(request.type_ == RequestType::Cancel){
            int wid =  std::get<CRQuery*>(request.payload_)->indexID_;
            workers_[wid]->submitTask(request);
        }else if(request.type_ == RequestType::Publish){
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    Logger::instance().writePhase("In experimental group for cancel test, PC-PS run time (seconds): ",
                                  canPubEndTime - canPubStartTime);

    // ---------- Step 3: Acquire the temporal duration of normal publishing requests process ----------
    resetWorkerThread(TestType::CanTest);
    double pubStartTime = thread_real_time(), pubEndTime;
    while (true) {
        Request request = generateNextRequestForPublishOnly();
        if(request.type_ == RequestType::Stop){
            for (auto& worker : workers_)
                worker->submitTask(request);
            for (auto& worker : workers_){
                worker->waitDone();
                if(!worker->index_->checkConsistency()) exit(0);
            }

            pubEndTime = thread_real_time();
            Result result;
            for(auto& worker: workers_){
                for(int idx = 0; idx < 9; ++idx){
                    result.resList_[idx] += worker->result_.resList_[idx];
                }
                result.total_ += worker->result_.total_;
            }
            cout << "IN R-BVH CANCEL LATENCY TEST (PHASE CTRL-GROUP):" << endl << "PC-PS RUN ";
            printf("%.3lf ", pubEndTime - pubStartTime);
            cout << "SECS ON AVERAGE AT " << PCPS::parallelism_ << " THREADS." << endl << endl;
            result.printResult();
            break;
        }else{
            for (auto& worker : workers_)
                worker->submitTask(request);
        }
    }

    Logger::instance().writePhase("In controlled group for cancel test, PC-PS run time (seconds): ",
                                  pubEndTime - pubStartTime);

    double buildTime = buildEndTime - buildStartTime;
    double canPubTime = canPubEndTime - canPubStartTime;
    double pubTime = pubEndTime - pubStartTime;

    cout << endl << endl;
    cout << "R-BVH PUBLISHING TEST COMPLETE" << endl;
    printf("TIME FOR PRE-BUILD (NOT USEFUL)    : %.3lf SECS\n", buildTime);
    printf("TIME FOR EXP-GROUP REQUEST PROCESS : %.3lf SECS\n", canPubTime);
    printf("TIME FOR CTRL-GROUP REQUEST PROCESS: %.3lf SECS\n", pubTime);
    cout << endl << endl;
    printf("NET-TIME FOR R-BVH QUERY DELETION: %.3lf SECS\n", canPubTime - pubTime);
    cout << endl << endl;

    Logger::instance().writeResult("Time for pre-build (seconds)", buildTime);
    Logger::instance().writeResult("Time for experimental group request process (seconds)", canPubTime);
    Logger::instance().writeResult("Time for controlled group request process (seconds)", pubTime);
    Logger::instance().writeLine("\n");
    Logger::instance().writeResult("Net-time for R-BVH query deletion", canPubTime - pubTime);
}