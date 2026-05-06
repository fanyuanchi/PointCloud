#include "RegularGrid.h"

int RegularGrid::gMin_ = 100;
int RegularGrid::gMax_ = 200;
double RegularGrid::ratioForDelete_ = 0.05;

void getCellRanges(int curDim, vector<int>& cur, const vector<int>& startIDX, const vector<int>& endIDX, int N,
                   vector<CellRange>& ranges) {
    if (curDim == DIM-1) {
        size_t base = 0;
        for (int k = 0; k < DIM-1; k++) {
            base += cur[k] * pow(N, DIM - 1 - k);
        }
        size_t L = base + startIDX[DIM-1];
        size_t R = base + endIDX[DIM-1];
        bool edge = false;
        for (int d = 0; d < DIM-1; d++) {
            if (cur[d] == startIDX[d] || cur[d] == endIDX[d]) {
                edge = true;
                break;
            }
        }
        RangeType type = edge ? RangeType::edgeRange : RangeType::internalRange;
        ranges.push_back({L, R, type});
        return;
    }

    for (int c = startIDX[curDim]; c <= endIDX[curDim]; c++) {
        cur[curDim] = c;
        getCellRanges(curDim + 1, cur, startIDX, endIDX, N, ranges);
    }
}

void RegularGrid::insertQuery(CRQuery& query){
    int queryID = query.queryID_;
    int regNum = 1;
    vector<int> startIDX(DIM), endIDX(DIM);
    for(int dim = 0; dim < DIM; ++dim){
        startIDX[dim] = static_cast<int>((query.low_[dim] - low_[dim]) / cellLength_[dim]);
        endIDX[dim] = static_cast<int>((query.top_[dim] - low_[dim]) / cellLength_[dim]);
        if (startIDX[dim] < 0) startIDX[dim] = 0;
        if (startIDX[dim] >= curG_) return;
        if (endIDX[dim] >= curG_) endIDX[dim] = curG_ - 1;
        if (endIDX[dim] < 0) return;
        regNum *= (endIDX[dim] - startIDX[dim] + 1);
    }
    vector<int> cur(DIM);
    vector<CellRange> ranges;
    getCellRanges(0, cur, startIDX, endIDX, curG_, ranges);
    for(auto& range: ranges){
        if(range.type == RangeType::edgeRange){
            for(size_t idx = range.L; idx <= range.R; ++idx){
                cells_[idx].addPart(queryID);
            }
        }else{
            if(range.L < range.R){
                cells_[range.L].addPart(queryID);
                cells_[range.R].addPart(queryID);
                for(size_t idx = range.L + 1; idx < range.R; ++idx){
                    cells_[idx].addFull(queryID);
                }
            }else{
                cells_[range.L].addPart(queryID);
            }
        }
    }
    queries_[queryID]->regiCounter_ = regNum;
}

void RegularGrid::condenseTree(double curTime) {
    if(curG_ <= RegularGrid::gMin_)
        throw std::runtime_error("cannot condense any more");
    curG_ -= (RegularGrid::gMax_ - RegularGrid::gMin_) / 4;
    rebuild();
}

void RegularGrid::rebuild() {
    cells_.clear();
    cells_.resize(static_cast<size_t>(pow(curG_, DIM)));
    cellLength_.resize(DIM);
    for(int dim = 0; dim < DIM; ++dim){
        cellLength_[dim] = (top_[dim] - low_[dim]) / curG_;
    }
    regCounter_ = 0;
    delNum_ = 0;
    for(auto& query: queries_){
        if(!query->isDelete_){
            query->regiCounter_ = 0;
            insertQuery(*query);
            regCounter_ += query->regiCounter_;
        }else if(query->regiCounter_ > 0){
            freeQueryID_.push(query->queryID_);
            query->regiCounter_ = 0;
        }
    }
}

void RegularGrid::registerCRQuery(CRQuery* query) {
    if (cells_.empty()) {
        size_t totalCellNum = 1;
        for(int dim = 0; dim < DIM; ++dim){
            totalCellNum *= curG_;
        }
        cells_.resize(totalCellNum);
    }
    int queryID = createQuery(query);
    insertQuery(*query);
    regCounter_ += queries_[queryID]->regiCounter_;
}

void RegularGrid::cancelCRQuery(CRQuery* query) {
    int qid = query->queryID_;
    if (!query->isEqual(*queries_[qid])) {
        throw std::runtime_error("unequal target query");
    }
    if (qid < 0 || qid >= queries_.size()) {
        cout << "queryID: " << query->queryID_ << "; query list size: " << queries_.size() << endl;
        throw std::runtime_error("queryID out of range.");
    }
    if (queries_[qid]->isDelete_) {
        cout << "queryID: " << query->queryID_ << endl;
        throw std::runtime_error("query already deleted.");
    }

    queries_[qid]->isDelete_ = true;
    result_.addResult(queries_[qid]->mesaCounter_);

    if(++delNum_ > (queries_.size() - freeQueryID_.size()) * RegularGrid::ratioForDelete_){
        fullScan2Del();
        delNum_ = 0;
    }
}

void RegularGrid::fullScan2Del(){
    for(auto& cell: cells_){
        auto& queryIDPartList = cell.partCover_;
        if(!queryIDPartList.empty()){
            int left = 0, right = queryIDPartList.size()-1;
            while(left < right){
                while(left < right && !queries_[queryIDPartList[left]]->isDelete_) ++left;
                while(left < right && queries_[queryIDPartList[right]]->isDelete_) --right;
                swap(queryIDPartList[left], queryIDPartList[right]);
            }
            if(queries_[queryIDPartList[left]]->isDelete_){
                regCounter_ -= (queryIDPartList.size() - left);
                queryIDPartList.erase(queryIDPartList.begin()+left, queryIDPartList.end());
            }
        }

        auto& queryIDFullList = cell.fullCover_;
        if(!queryIDFullList.empty()){
            int left = 0, right = queryIDFullList.size()-1;
            while(left < right){
                while(left < right && !queries_[queryIDFullList[left]]->isDelete_) ++left;
                while(left < right && queries_[queryIDFullList[right]]->isDelete_) --right;
                swap(queryIDFullList[left], queryIDFullList[right]);
            }
            if(queries_[queryIDFullList[left]]->isDelete_){
                regCounter_ -= (queryIDFullList.size() - left);
                queryIDFullList.erase(queryIDFullList.begin()+left, queryIDFullList.end());
            }
        }
    }

    for(auto& query: queries_)
        if(query->isDelete_ && query->regiCounter_ > 0) {
            freeQueryID_.push(query->queryID_);
            query->regiCounter_ = 0;
        }
}

void RegularGrid::publishPoint(Point* point) {
    size_t cordIDX = 0;
    for(int dim = 0; dim < DIM; ++dim){
        int gridIDX = static_cast<int>((point->cord_[dim] - low_[dim]) / cellLength_[dim]);
        if(gridIDX < 0 || gridIDX >= curG_) return;
        cordIDX += gridIDX * static_cast<int>(pow(curG_, DIM - dim - 1));
    }

    for (auto queryID : cells_[cordIDX].partCover_) {
        if(!queries_[queryID]->isDelete_ && queries_[queryID]->isContain(*point))
            queries_[queryID]->recvMesa();
    }
    for (auto queryID : cells_[cordIDX].fullCover_) {
        if(!queries_[queryID]->isDelete_)
            queries_[queryID]->recvMesa();
    }
}

bool RegularGrid::checkConsistency() const {
    size_t regCounterInIndex = 0;
    size_t regCounterQuerySum = 0;

    for(auto& cell: cells_){
        regCounterInIndex += (cell.partCover_.size() + cell.fullCover_.size());
    }
    for(auto& query: queries_)
        regCounterQuerySum += query->regiCounter_;

    if(regCounter_ != regCounterQuerySum ||
       regCounter_ != regCounterInIndex) {
        std::cerr << "[Regular Grid Consistency Error]\n"
                  << " regCounter_          = " << regCounter_ << "\n"
                  << " regCounterInIndex    = " << regCounterInIndex << "\n"
                  << " regCounterQuerySum   = " << regCounterQuerySum << "\n";
        return false;
    }
    assert(regCounter_ == regCounterQuerySum && regCounter_ == regCounterInIndex);
    return true;
}
