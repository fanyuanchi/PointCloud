#ifndef PCPS_IINDEX_H
#define PCPS_IINDEX_H

#include "Geometry.h"

struct Result{
    vector<int> resList_;
    size_t total_;
    Result(): resList_(9, 0), total_(0){}

    void addResult(int messageNum){
        total_ += messageNum;
        if(messageNum == 0){
            ++resList_[0];
        }else if(messageNum < 10){
            ++resList_[1];
        }else if(messageNum < 100){
            ++resList_[2];
        }else if(messageNum < 1000){
            ++resList_[3];
        }else if(messageNum < 10000){
            ++resList_[4];
        }else if(messageNum < 100000){
            ++resList_[5];
        }else if(messageNum < 1000000){
            ++resList_[6];
        }else if(messageNum < 10000000){
            ++resList_[7];
        }else{
            ++resList_[8];
        }
    }

    void printResult(){
        cout << endl;
        cout << "RESULT INFORMATION: " << endl;
        cout << "TOTAL MATCH: " << total_ << endl;
        cout << "0  result: " << resList_[0] << endl;
        cout << "1   ~ 10 : " << resList_[1] << endl;
        cout << "10  ~ 100: " << resList_[2] << endl;
        cout << "100 ~ 1e3: " << resList_[3] << endl;
        cout << "1e3 ~ 1e4: " << resList_[4] << endl;
        cout << "1e4 ~ 1e5: " << resList_[5] << endl;
        cout << "1e5 ~ 1e6: " << resList_[6] << endl;
        cout << "1e6 ~ 1e7: " << resList_[7] << endl;
        cout << "   MORE  : " << resList_[8] << endl;
        cout << endl;
    }

    void resetResult(){
        total_ = 0;
        resList_.assign(resList_.size(), 0);
    }
};

class IIndex {
public:
    vector<CRQuery*> queries_;
    queue<int> freeQueryID_;
    size_t regCounter_ = 0;
    Result result_;
    virtual ~IIndex() {
        for(auto queryPtr: queries_)
            delete queryPtr;
    }

    int createQuery(CRQuery* query) {
        int queryID;
        if(freeQueryID_.empty()){
            queryID = queries_.size();
            query->queryID_ = queryID;
            auto* newQuery = new CRQuery(*query);
            queries_.push_back(newQuery);
        }else{
            queryID = freeQueryID_.front();
            freeQueryID_.pop();
            query->queryID_ = queryID;
            queries_[queryID]->copyQuery(*query);
        }
        return queryID;
    }

    virtual void registerCRQuery(CRQuery* query) = 0;
    virtual void cancelCRQuery(CRQuery* query) = 0;
    virtual void publishPoint(Point* point) = 0;
    // Check internal consistency of the index.
    // Must verify that:
    // 1) regCounter_ equals the total number of stored registrations
    // 2) Sum of queries_[i].regiCounter_ equals regCounter_
    // 3) No deleted query appears in index storage
    // Returns true if consistent, false otherwise.
    virtual bool checkConsistency() const = 0;

    Result totalMatch() {
        for(auto& query: queries_){
            if(query->isDelete_) continue;
            result_.addResult(query->mesaCounter_);
        }
        return result_;
    };
    void resetResult(){
        for(auto& query: queries_)
            query->mesaCounter_ = 0;
        result_.resetResult();
    }
    // ---- index-specific test extensions  ----
    // Default no-op. Only meaningful for specific index (R-BVH and GridFamily indexes) tests .
    virtual void flush(){};
    virtual void condenseTree(double curTime) {};
    virtual void updateSTList(const Point* p, double curTime) {};
    virtual float getFlushInterval() {return 100;};

    // Only meaningful for R-BVH incremental construction, do not do any matching operation
    // Operations "publishFull" and "publishPart" are denied
    virtual bool incrementalConstruction(Point* point) { return 0.0;};
    // Only meaningful for R-BVH publishing synchronized query deletion, do not change R-BVH structure
    // Operation "splitNode" is denied
    virtual void publishSynchronizedDelete(Point* point) {};
};

#endif //PCPS_IINDEX_H
