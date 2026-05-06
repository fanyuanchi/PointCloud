#ifndef PCPS_WORKER_H
#define PCPS_WORKER_H

#include "../util/UTIL.h"
#include "../index/Geometry.h"
#include "../index/R-BVH/R-BVH.h"
#include "../index/BruteForce/Brute.h"
#include "../index/R-TreeFamily/R-Tree/R-Tree.h"
#include "../index/R-TreeFamily/RS-Tree/RS-Tree.h"
#include "../index/R-TreeFamily/RRS-Tree/RRS-Tree.h"
#include "../index/R-TreeFamily/RLR-Tree/RLR-Tree.h"
#include "../index/GridFamily/RegularGrid/RegularGrid.h"
#include "../index/GridFamily/Z-2DGrid/Z-2DGrid.h"

enum RequestType{None, Register, Cancel, Publish, Stop};
enum IndexType{RBvh, RTree, RSTree, RRSTree, RLRTree, RegularGrid, ZGrid, BruteForce};
enum TestType{Baseline, RegTest, CanTest, PubTest, Throughput};

const unordered_map<enum IndexType, string> indexNames =
        {
                {IndexType::RBvh, "R-BVH"},
                {IndexType::RTree, "R-Tree"},
                {IndexType::RSTree, "R*-Tree"},
                {IndexType::RRSTree, "RR*-Tree"},
                {IndexType::RLRTree, "RLR-Tree"},
                {IndexType::RegularGrid, "RegularGrid"},
                {IndexType::ZGrid, "Z-2DGrid"},
                {IndexType::BruteForce, "Brute-Force"}
        };

const unordered_map<enum TestType, string> testNames =
        {
                {TestType::Baseline, "BaselineTest"},
                {TestType::Throughput, "ThroughputTest"},
                {TestType::RegTest, "RegistrationTest"},
                {TestType::PubTest, "PublishingTest"},
                {TestType::CanTest, "QueryDeletionTest"}
        };

class Request{
public:
    enum RequestType type_;
    std::variant<Point*, CRQuery*> payload_;

    Request(): type_(RequestType::None){}
    Request(RequestType t): type_(t){}
    Request(Point* p): type_(RequestType::Publish){
        payload_.emplace<Point*>(p);
    }
    Request(RequestType t, CRQuery* q): type_(t){
        payload_.emplace<CRQuery*>(q);
    }

    ~Request() = default;

    void CopyRequest(const Request &request){
        type_ = request.type_;
        payload_ = request.payload_;
    }
};

class Worker {
public:
    int workerID_;
    std::thread th_;
    queue<Request> requests_;

    IndexType indexType_;
    std::unique_ptr<IIndex> index_ = nullptr;

    Result result_;
    double memory_ = 0.0;
    double peak_ = 0.0;
    double runTime_ = 0.0;
    double stageTime_[3] = {0.0, 0.0, 0.0};

    std::mutex mtx_;
    std::condition_variable cv_;

    static double maxM_;

    Worker(const int ID, const vector<double>& LOW, const vector<double>& TOP, enum IndexType indexType, enum TestType testType) :
            workerID_(ID), indexType_(indexType){
        assert(LOW.size() == DIM && TOP.size() == DIM);
        if(indexType == IndexType::RBvh){
            index_ = std::make_unique<RBVH>(LOW, TOP);
            if(testType == TestType::Throughput){
                th_ = std::thread(&Worker::runRBVHThroughputTest, this);
            }else if(testType == TestType::RegTest){
                th_ = std::thread(&Worker::runRBVHRegTest, this);
            }else if(testType == TestType::PubTest){
                th_ = std::thread(&Worker::runRBVHPubTest, this);
            }else if(testType == TestType::CanTest){
                th_ = std::thread(&Worker::runRBVHCanTest, this);
            }else{
                cout << "current test type: " << testNames.find(testType)->second << endl;
                throw std::runtime_error("wrong test type for R-BVH");
            }
        }else{
            if(indexType_ == IndexType::BruteForce){
                index_ = std::make_unique<Brute>();
            }else if(indexType_ == IndexType::RTree){
                index_ = std::make_unique<class::RTree>();
            }else if(indexType_ == IndexType::RSTree){
                index_ = std::make_unique<class::RSTree>();
            }else if(indexType_ == IndexType::RRSTree){
                index_ = std::make_unique<class::RRSTree>();
            }else if(indexType_ == IndexType::RLRTree){
                index_ = std::make_unique<class::RLRTree>();
            }else if(indexType_ == IndexType::RegularGrid){
                index_ = std::make_unique<class::RegularGrid>(LOW, TOP);
            }else if(indexType_ == IndexType::ZGrid){
                index_ = std::make_unique<class::Z2DGrid>(LOW, TOP);
            }else{
                cout << "current index type: " << testNames.find(testType)->second << endl;
                throw std::runtime_error("wrong index type");
            }
            th_ = std::thread(&Worker::runBaselineTest, this);
        }
    }
    ~Worker(){
        if (th_.joinable())
            th_.join();
    }

    void submitTask(const Request &request) {
        {
            std::unique_lock<std::mutex> lk(mtx_);
            requests_.push(request);
        }
        cv_.notify_one();
    }

    void waitDone() {
        if (th_.joinable())
            th_.join();
        assert(requests_.empty());
    }

    void resetThread(enum TestType testType){
        if(indexType_ != IndexType::RBvh){
            cout << "Needed index type: R-Bvh, current index type: " << indexNames.find(indexType_)->second << endl;
            throw std::runtime_error("wrong index type");
        }
        if (th_.joinable()) th_.join();
        if(testType == TestType::Throughput){
            th_ = std::thread(&Worker::runRBVHThroughputTest, this);
        }else if(testType == TestType::RegTest){
            th_ = std::thread(&Worker::runRBVHRegTest, this);
        }else if(testType == TestType::PubTest){
            th_ = std::thread(&Worker::runRBVHPubTest, this);
        }else if(testType == TestType::CanTest){
            th_ = std::thread(&Worker::runRBVHCanTest, this);
        }else{
            cout << "current test type: " << testNames.find(testType)->second << endl;
            throw std::runtime_error("wrong test type for R-BVH");
        }
    }

    void runRBVHThroughputTest();
    void runBaselineTest();

    void runRBVHRegTest();
    void runRBVHPubTest();
    void runRBVHCanTest();
};

#endif //PCPS_WORKER_H
