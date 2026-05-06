#include "Brute.h"

void Brute::registerCRQuery(CRQuery* query) {
    ++regCounter_;
    int queryID = createQuery(query);
    queries_[queryID]->regiCounter_ = 1;
    queryIDList_.push_back(queryID);
}

void Brute::cancelCRQuery(CRQuery* query) {
    int qid = query->queryID_;
    if(!query->isEqual(*queries_[qid])){
        throw std::runtime_error("unequal target query");
    }
    if(qid < 0 || qid >= queries_.size()){
        cout << "queryID: " << query->queryID_ << "; query list size: " << queries_.size() << endl;
        throw std::runtime_error("queryID out of range.");
    }
    if(queries_[qid]->isDelete_) {
        cout << "queryID: " << query->queryID_ << endl;
        throw std::runtime_error("query already deleted.");
    }

    queries_[qid]->isDelete_ = true;
    queries_[qid]->regiCounter_ = 0;
    freeQueryID_.push(qid);
    --regCounter_;

    for (size_t i = 0; i < queryIDList_.size(); ++i) {
        if (queryIDList_[i] == qid) {
            queryIDList_[i] = queryIDList_.back();
            queryIDList_.pop_back();
            break;
        }
    }

    result_.addResult(queries_[qid]->mesaCounter_);
}

void Brute::publishPoint(Point* point) {
    for(auto& queryID: queryIDList_){
        if(queries_[queryID]->isDelete_) continue;
        if(queries_[queryID]->isContain(*point))
            queries_[queryID]->recvMesa();
    }
}

bool Brute::checkConsistency() const {
    long long int regCounterInIndex = queryIDList_.size();
    long long int regCounterQuerySum = 0;
    for(auto& query: queries_)
        regCounterQuerySum += query->regiCounter_;

    if(regCounter_ != regCounterQuerySum ||
       regCounter_ != regCounterInIndex) {
        std::cerr << "[Brute-Force Consistency Error]\n"
                  << " regCounter_          = " << regCounter_ << "\n"
                  << " regCounterInIndex    = " << regCounterInIndex << "\n"
                  << " regCounterQuerySum   = " << regCounterQuerySum << "\n";
        return false;
    }
    assert(regCounter_ == regCounterQuerySum && regCounter_ == regCounterInIndex);
    return true;
}
