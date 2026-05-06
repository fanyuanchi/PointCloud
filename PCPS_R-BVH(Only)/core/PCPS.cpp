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


void PCPS::runForRBVHRegTest() {
    resetIndex();

    vector<thread> regThreads;
    regThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        regThreads.emplace_back(&PCPS::regQuery, this, idx);
    for(auto& regThread: regThreads)
        regThread.join();

    vector<thread> conThreads;
    conThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        conThreads.emplace_back(&PCPS::incrementalConstruct, this, idx);
    for(auto& pubThread: conThreads)
        pubThread.join();

    vector<thread> emptyThreads;
    emptyThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        emptyThreads.emplace_back(&PCPS::emptyPub, this, idx);
    for(auto& emptyThread: emptyThreads)
        emptyThread.join();

    double averagedRegTime = 0.0, totalRegTime = 0.0;
    for(int idx = 0; idx < PCPS::parallelism_; ++idx){
        averagedRegTime += timer_[idx] / PCPS::parallelism_;
        totalRegTime = max(totalRegTime, timer_[idx]);
    }

    cout << endl << endl;
    printf("PC-PS RUN %.3lf / %.3lf ", averagedRegTime, totalRegTime);
    cout << endl << endl;
}

/**
 * The R-BVH Publishing Latency Test contains the following three steps:
 * 2) Step 1: Do Pre-Registration and Incremental Construction to prebuild R-BVH the fix the structure during the next step
 * 3) Step 2: Redo Normal Publishing Request Process Test to acquire the net latency
 *            (only publishing request process and matching operation) requests
 */
void PCPS::runForRBVHPubTest(){
    resetIndex();

    vector<thread> regThreads;
    regThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        regThreads.emplace_back(&PCPS::regQuery, this, idx);
    for(auto& regThread: regThreads)
        regThread.join();

    vector<thread> conThreads;
    conThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        conThreads.emplace_back(&PCPS::incrementalConstruct, this, idx);
    for(auto& conThread: conThreads)
        conThread.join();

    for(int idx = 0; idx < PCPS::parallelism_; ++idx){
        timer_[idx] = 0.0;
    }

    vector<thread> pubThreads;
    pubThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        pubThreads.emplace_back(&PCPS::pubPoint, this, idx);
    for(auto& pubThread: pubThreads)
        pubThread.join();

    double averagedPubTime = 0.0, totalPubTime = 0.0;
    for(int idx = 0; idx < PCPS::parallelism_; ++idx){
        averagedPubTime += timer_[idx] / PCPS::parallelism_;
        totalPubTime = max(totalPubTime, timer_[idx]);
    }

    double averagedPubThroughput = averagedPubTime * 1e3 / PCPS::publishNum_;
    double totalPubThroughput = totalPubTime * 1e3 / PCPS::publishNum_;

    cout << endl << endl;
    printf("PC-PS RUN %.3lf / %.3lf ", averagedPubTime, totalPubTime);
    printf("R-BVH PUBLISHING THROUGHPUT: %.3lf / %.3lf SECS PER 1K POINTS\n", averagedPubThroughput, totalPubThroughput);
    cout << endl << endl;
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
    resetIndex();

    vector<thread> regThreads;
    regThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        regThreads.emplace_back(&PCPS::regQuery, this, idx);
    for(auto& regThread: regThreads)
        regThread.join();

    vector<thread> conThreads;
    conThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        conThreads.emplace_back(&PCPS::incrementalConstruct, this, idx);
    for(auto& conThread: conThreads)
        conThread.join();

    vector<thread> checkThreads;
    checkThreads.reserve(PCPS::parallelism_);
    for(int idx = 0; idx < PCPS::parallelism_; ++idx)
        checkThreads.emplace_back(&PCPS::checkStructure, this, idx);
    for(auto& checkThread: checkThreads)
        checkThread.join();


    double averagedCanTime = 0.0, totalCanTime = 0.0;
    for(int idx = 0; idx < PCPS::parallelism_; ++idx){
        averagedCanTime += timer_[idx] / PCPS::parallelism_;
        totalCanTime = max(totalCanTime, timer_[idx]);
    }
    cout << endl << endl;
    printf("PC-PS RUN %.3lf / %.3lf ", averagedCanTime, totalCanTime);
    cout << endl << endl;
}

void PCPS::regQuery(int idx){
    double start = thread_real_time();
    for(int queryID = idx; queryID < PCPS::registerNum_; queryID += PCPS::parallelism_){
        indexes_[idx]->registerCRQuery(queries_[queryID]);
        if((queryID+1) % 100000 == 0){
            cout << "Thread " << idx << " reg 100k /" << (queryID+1) / 1000 << "k queries" << endl << endl;
        }
    }

    timer_[idx] += thread_real_time() - start;
}

void PCPS::pubPoint(int idx){
    double start = thread_real_time();
    for(int pointID = 0; pointID < PCPS::publishNum_; ++pointID){
        indexes_[idx]->publishPoint(points_[pointID]);
        if((pointID+1) % 100000 == 0){
            cout << "Thread " << idx << " pub 100k / " << (pointID+1) / 1000 << "k points" << endl << endl;
        }
    }

    timer_[idx] += thread_real_time() - start;
}

void PCPS::incrementalConstruct(int idx){
    double start = thread_real_time();
    for(int pointID = 0; pointID < PCPS::publishNum_; ++pointID){
        indexes_[idx]->incrementalConstruction(points_[pointID]);
        if((pointID+1) % 100000 == 0){
            cout << "Thread " << idx << " pub 100k / " << (pointID+1) / 1000 << "k points" << endl << endl;
        }
    }

    timer_[idx] += thread_real_time() - start;
}

void PCPS::emptyPub(int idx){
    double start = thread_real_time();
    for(int pointID = 0; pointID < PCPS::publishNum_; ++pointID){
        if((pointID+1) % 100000 == 0){
            cout << "Thread " << idx << " pub 100k / " << (pointID+1) / 1000 << "k points" << endl << endl;
        }
    }

    timer_[idx] -= thread_real_time() - start;
}

void PCPS::checkStructure(int idx){
    double start = thread_real_time();
    const int childNum = 1 << DIM;
    queue<RBVHNode*> Q;
    RBVHNode *tmp;
    Q.push(indexes_[idx]->root_);
    while(!Q.empty()){
        tmp = Q.front();
        Q.pop();
        for(auto& q: tmp->fullCover_){
            continue;
        }

        for(auto& q: tmp->partCover_){
            continue;
        }

        if(tmp->children_ != nullptr)
            for(int childID = 0; childID < childNum; ++childID)
                Q.push(tmp->children_[childID]);
    }
    for(auto& query: queries_)
        continue;

    timer_[idx] = thread_real_time() - start;
}