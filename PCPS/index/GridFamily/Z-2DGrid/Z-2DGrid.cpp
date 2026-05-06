#include "Z-2DGrid.h"

int Grid2D::gMin_ = 100;
int Grid2D::gMax_ = 300;

int Z2DGrid::hMax_ = 9;
int Z2DGrid::rMin_ = 64;
double Z2DGrid::ratioForDelete_ = 0.05;
double Z2DGrid::ratioForSplit_ = 0.05;

void Z2DGrid::registerCRQuery(CRQuery* query) {
    ++curAliveQueryNum_;
    int queryID = createQuery(query);
    queue<TreeNode*> Q, S;
    Q.push(root_);
    while(!Q.empty()){
        TreeNode* node = Q.front();
        Q.pop();
        if(query->low_[2] > node->zMax_ || query->top_[2] < node->zMin_) continue;
        // leaf node
        if(node->gridIndex_ == nullptr) {
            node->leafList_.push_back(queryID);
            ++regCounter_;
            ++queries_[queryID]->regiCounter_;
            if(node->height_ < Z2DGrid::hMax_ - 1 && node->leafList_.size()
            > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
                S.push(node);
            }
            continue;
        }
        // non-leaf node
        if(query->low_[2] < node->zMid_ && node->zMid_ < query->top_[2]){
            insertQuery(node, queries_[queryID]);
        }else{
            Q.push(node->leftChild_);
            Q.push(node->rightChild_);
        }
    }

    while(!S.empty()){
        TreeNode* node = S.front();
        S.pop();
//        assert(node->gridIndex_ == nullptr);
        splitNode(node);
        if(node->leftChild_->height_ < Z2DGrid::hMax_ - 1 && node->leftChild_->leafList_.size()
           > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
            S.push(node->leftChild_);
        }
        if(node->rightChild_->height_ < Z2DGrid::hMax_ - 1 && node->rightChild_->leafList_.size()
           > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
            S.push(node->rightChild_);
        }
    }
}

void Z2DGrid::splitNode(TreeNode* node) {
    node->leftChild_ = new TreeNode(node, true);
    node->rightChild_ = new TreeNode(node, false);
    vector<int> leftQueries, rightQueries, midQueries;
    for (auto& queryID: node->leafList_) {
        --queries_[queryID]->regiCounter_;
        if(queries_[queryID]->isDelete_) continue;
        if (queries_[queryID]->top_[2] <= node->zMid_) { // move to the left child
            leftQueries.push_back(queryID);
        } else if (queries_[queryID]->low_[2] >= node->zMid_) { // move to the right child
            rightQueries.push_back(queryID);
        } else { // remain in the branch node
            midQueries.push_back(queryID);
        }
    }
    regCounter_ -= node->leafList_.size();
    node->leafList_.clear();
    node->gridIndex_ = new Grid2D(curG_);
    for(auto& queryID: midQueries)
        insertQuery(node, queries_[queryID]);
    for(auto& queryID: leftQueries){
        ++queries_[queryID]->regiCounter_;
        node->leftChild_->leafList_.push_back(queryID);
    }
    for(auto& queryID: rightQueries){
        ++queries_[queryID]->regiCounter_;
        node->rightChild_->leafList_.push_back(queryID);
    }
    regCounter_ += leftQueries.size() + rightQueries.size();
}

void Z2DGrid::insertQuery(TreeNode* node, CRQuery* query){
//    assert(node->gridIndex_ != nullptr);
    vector<pair<size_t,size_t>> ranges;
    int regNum = getCellRanges(query, ranges);
    for(int idx = 0; idx < ranges.size(); ++idx){
        auto range = ranges[idx];
        if(idx == 0 || idx == ranges.size()-1){
            for(size_t cell = range.first; cell <= range.second; ++cell)
                node->gridIndex_->cells_[cell].partCover_.push_back(query->queryID_);
        }else{
            if(range.first < range.second){
                node->gridIndex_->cells_[range.first].partCover_.push_back(query->queryID_);
                node->gridIndex_->cells_[range.second].partCover_.push_back(query->queryID_);
                for(size_t cell = range.first+1; cell < range.second; ++cell)
                    node->gridIndex_->cells_[cell].fullCover_.push_back(query->queryID_);
            }else{
                node->gridIndex_->cells_[range.first].partCover_.push_back(query->queryID_);
            }
        }
    }
    query->regiCounter_ += regNum;
    regCounter_ += regNum;
}

int Z2DGrid::getCellRanges(CRQuery* query, vector<pair<size_t,size_t>>& ranges){
    ranges.clear();
    int startIDX[2], endIDX[2];
    int regNum = 1;
    for (int dim: {0, 1}){
        double cellLength = (top_[dim] - low_[dim]) / curG_;
        int sid = static_cast<int>((query->low_[dim] - low_[dim]) / cellLength);
        int eid = static_cast<int>((query->top_[dim] - low_[dim]) / cellLength);

        if(sid < 0) sid = 0;
        if(sid >= curG_) return 0;
        if(eid >= curG_) eid = curG_-1;
        if(eid < 0) return 0;

        startIDX[dim] = sid;
        endIDX[dim] = eid;

        regNum *= (eid - sid + 1);
    }

    for(int idx = startIDX[0]; idx <= endIDX[0]; ++idx){
        ranges.emplace_back(idx * curG_ + startIDX[1], idx * curG_ + endIDX[1]);
    }
    return regNum;
}

void Z2DGrid::cancelCRQuery(CRQuery* query) {
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

    --curAliveQueryNum_;
    queries_[qid]->isDelete_ = true;
    result_.addResult(queries_[qid]->mesaCounter_);

    if(++delNum_ > curAliveQueryNum_ * Z2DGrid::ratioForDelete_){
        fullScan2Del();
        delNum_ = 0;
    }
}

void Z2DGrid::fullScan2Del(){
    queue<TreeNode*> Q;
    Q.push(root_);
    while(!Q.empty()){
        TreeNode* tmp = Q.front();
        Q.pop();
        if(tmp->gridIndex_ != nullptr){
            for(auto& cell: tmp->gridIndex_->cells_){
                auto& queryIDFullList = cell.fullCover_;
                if(!queryIDFullList.empty()) {
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

                auto& queryIDPartList = cell.partCover_;
                if(!queryIDPartList.empty()) {
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
            }
            Q.push(tmp->leftChild_);
            Q.push(tmp->rightChild_);
        }else{
            auto& queryIDList = tmp->leafList_;
            if(queryIDList.empty()) continue;
            int left = 0, right = queryIDList.size()-1;
            while(left < right){
                while(left < right && !queries_[queryIDList[left]]->isDelete_) ++left;
                while(left < right && queries_[queryIDList[right]]->isDelete_) --right;
                swap(queryIDList[left], queryIDList[right]);
            }
            if(queries_[queryIDList[left]]->isDelete_){
                regCounter_ -= (queryIDList.size() - left);
                queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
            }
        }
    }

    for(auto& query: queries_)
        if(query->isDelete_ && query->regiCounter_ > 0) {
            freeQueryID_.push(query->queryID_);
            query->regiCounter_ = 0;
        }
}

void Z2DGrid::rebuild(){
    delete root_;
    root_ = new TreeNode(low_[2], top_[2]);
    regCounter_ = 0;
    delNum_ = 0;
    for(auto& query: queries_){
        if(query->isDelete_) {
            if(query->regiCounter_ > 0){
                freeQueryID_.push(query->queryID_);
                query->regiCounter_ = 0;
            }
            continue;
        }
        query->regiCounter_ = 0;
        queue<TreeNode*> Q, S;
        Q.push(root_);
        while(!Q.empty()){
            TreeNode* node = Q.front();
            Q.pop();
            if(query->low_[2] > node->zMax_ || query->top_[2] < node->zMin_) continue;
            // leaf node
            if(node->gridIndex_ == nullptr) {
                node->leafList_.push_back(query->queryID_);
                ++regCounter_;
                ++query->regiCounter_;
                if(node->height_ < Z2DGrid::hMax_ - 1 && node->leafList_.size()
                   > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
                    S.push(node);
                }
                continue;
            }
            // non-leaf node
            if(query->low_[2] < node->zMid_ && node->zMid_ < query->top_[2]){
                insertQuery(node, query);
            }else{
                Q.push(node->leftChild_);
                Q.push(node->rightChild_);
            }
        }

        while(!S.empty()){
            TreeNode* node = S.front();
            S.pop();
//            assert(node->gridIndex_ == nullptr);
            splitNode(node);
            if(node->leftChild_->height_ < Z2DGrid::hMax_ - 1 && node->leftChild_->leafList_.size()
               > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
                S.push(node->leftChild_);
            }
            if(node->rightChild_->height_ < Z2DGrid::hMax_ - 1 && node->rightChild_->leafList_.size()
               > max(Z2DGrid::rMin_, static_cast<int>(Z2DGrid::ratioForSplit_ * curAliveQueryNum_))){
                S.push(node->rightChild_);
            }
        }
    }
}

void Z2DGrid::publishPoint(Point* point) {
    TreeNode* node = root_;
    int cordIDX[2];
    bool tag = true;
    for (int dim: {0, 1}){
        double cellLength = (top_[dim] - low_[dim]) / curG_;
        int cid = static_cast<int>((point->cord_[dim] - low_[dim]) / cellLength);
        if(cid < 0 || cid >= curG_) {
            tag = false;
            break;
        }
        cordIDX[dim] = cid;
    }
    while(true){
        if(node->gridIndex_ == nullptr){
            for(auto& queryID: node->leafList_)
                if(!queries_[queryID]->isDelete_ && queries_[queryID]->isContain(*point))
                    queries_[queryID]->recvMesa();
            break;
        }else{
            if(tag){
                for(auto& queryID: node->gridIndex_->cells_[cordIDX[0] * curG_ + cordIDX[1]].fullCover_){
                    if(!queries_[queryID]->isDelete_
                       && queries_[queryID]->low_[2] <= point->cord_[2] && point->cord_[2] <= queries_[queryID]->top_[2])
                        queries_[queryID]->recvMesa();
                }
                for(auto& queryID: node->gridIndex_->cells_[cordIDX[0] * curG_ + cordIDX[1]].partCover_){
                    if(!queries_[queryID]->isDelete_ && queries_[queryID]->isContain(*point))
                        queries_[queryID]->recvMesa();
                }
            }

            if(point->cord_[2] <= node->zMid_)
                node = node->leftChild_;
            else
                node = node->rightChild_;
        }
    }
}

void Z2DGrid::condenseTree(double curTime) {
    if(curG_ <= Grid2D::gMin_)
        throw std::runtime_error("cannot condense any more");
    curG_ -= (Grid2D::gMax_ - Grid2D::gMin_) / 4;
    rebuild();
}

bool Z2DGrid::checkConsistency() const {
    size_t regCounterInIndex = 0;
    size_t regCounterQuerySum = 0;

    queue<TreeNode*> Q;
    Q.push(root_);
    while(!Q.empty()){
        TreeNode* tmp = Q.front();
        Q.pop();
        if(tmp->gridIndex_ != nullptr){
//            assert(tmp->leftChild_ != nullptr && tmp->rightChild_ != nullptr);
            for(auto& cell: tmp->gridIndex_->cells_){
                regCounterInIndex += cell.fullCover_.size();
                regCounterInIndex += cell.partCover_.size();
            }
            Q.push(tmp->leftChild_);
            Q.push(tmp->rightChild_);
        }else{
            regCounterInIndex += tmp->leafList_.size();
        }
    }
    for(auto& query: queries_)
        regCounterQuerySum += query->regiCounter_;

    if(regCounter_ != regCounterQuerySum ||
       regCounter_ != regCounterInIndex) {
        std::cerr << "[Z-2D Grid Consistency Error]\n"
                  << " regCounter_          = " << regCounter_ << "\n"
                  << " regCounterInIndex    = " << regCounterInIndex << "\n"
                  << " regCounterQuerySum   = " << regCounterQuerySum << "\n";
        return false;
    }
    assert(regCounter_ == regCounterQuerySum && regCounter_ == regCounterInIndex);
    return true;
}