#include "R-BVH.h"

int RBVHNode::maxB_ = 1024;
int RBVH::minH_ = 8;
int RBVH::maxH_ = 9;
double RBVH::minR_ = 0.01;
double RBVH::maxR_ = 0.05;
double RBVH::lambda_ = 0.1;
double RBVH::kappa_ = 10;
double RBVH::tau_ = 0.75;

void RBVH::splitNode(RBVHNode *node){
    static const int childNum = 1 << DIM;
    node->children_ = new RBVHNode*[childNum];
    for(int idx = 0; idx < childNum; ++idx){
        node->children_[idx] = new RBVHNode(idx, node);
    }
    enum Relation relation;
    RBVHNode *child;
    for(auto queryID: node->partCover_){
        --queries_[queryID]->regiCounter_;
        if(queries_[queryID]->isDelete_){
            if(queries_[queryID]->regiCounter_ == 0)
                freeQueryID_.push(queryID);
            continue;
        }
        for(int idx = 0; idx < childNum; ++idx){
            child = node->children_[idx];
            relation = child->getRelation(*queries_[queryID]);
            if(relation == Contained){
                child->fullCover_.push_back(queryID);
                ++queries_[queryID]->regiCounter_;
            }else if(relation != Disjoint){
                child->partCover_.push_back(queryID);
                ++queries_[queryID]->regiCounter_;
            }
        }
    }
    regCounter_ -= node->partCover_.size();
    for(int idx = 0; idx < childNum; ++idx){
        child = node->children_[idx];
        regCounter_ += child->fullCover_.size() + child->partCover_.size();
    }
    node->partCover_.clear();
}

void RBVH::registerCRQuery(CRQuery* query){
    ++curAliveQueryNum;
    static const int childNum = 1 << DIM;
    int queryID = createQuery(query);
    flushMap_[query->MLP_]++;
    queue<RBVHNode*> que;
    RBVHNode *node;
    enum Relation relation;
    que.push(root_);
    while(!que.empty()){
        node = que.front(); que.pop();
        relation = node->getRelation(*query);
        if(relation == Disjoint) continue;
        if(relation == Contained){
            node->fullCover_.push_back(queryID);
            ++queries_[queryID]->regiCounter_;
        }else{
            if(node->children_ == nullptr){
                node->partCover_.push_back(queryID);
                ++queries_[queryID]->regiCounter_;
            }else{
                for(int i = 0; i < childNum; ++i)
                    que.push(node->children_[i]);
            }
        }
    }
    regCounter_ += queries_[queryID]->regiCounter_;
}

void RBVH::cancelCRQuery(CRQuery* query) {
    int qid = query->queryID_;
    if(qid < 0 || qid >= queries_.size()){
        cout << "queryID: " << query->queryID_ << "; query list size: " << queries_.size() << endl;
        throw std::runtime_error("queryID out of range.");
    }
    --curAliveQueryNum;
    auto it = flushMap_.find(query->MLP_);
    if(it != flushMap_.end()){
        it->second--;
        if(it->second == 0) flushMap_.erase(it);
    }
    result_.addResult(queries_[qid]->mesaCounter_);
    queries_[qid]->isDelete_ = true;
}

void RBVH::publishFull(RBVHNode *node){
    if(++node->buffer_ < RBVHNode::maxB_) return;
    auto &queryIDList = node->fullCover_;
    node->buffer_ = 0;
    if(queryIDList.empty()) return;
    int b = RBVHNode::maxB_;
    int left = 0, right = queryIDList.size()-1;
    while(left < right){
        while(left < right && !queries_[queryIDList[left]]->isDelete_){
            queries_[queryIDList[left]]->recvMesa(b);
            ++left;
        }
        while(left < right && queries_[queryIDList[right]]->isDelete_){
            if(--queries_[queryIDList[right]]->regiCounter_ == 0)
                freeQueryID_.push(queryIDList[right]);
            --right;
        }
        swap(queryIDList[left], queryIDList[right]);
    }
    if(queries_[queryIDList[left]]->isDelete_){
        if(--queries_[queryIDList[left]]->regiCounter_ == 0)
            freeQueryID_.push(queryIDList[left]);
        regCounter_ -= (queryIDList.size() - left);
        queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
    }else{
        queries_[queryIDList[left]]->recvMesa(b);
    }
}

void RBVH::publishPart(RBVHNode *node, Point* p){
    auto &queryIDList = node->partCover_;
    if(queryIDList.empty()) return;
    int left = 0, right = queryIDList.size()-1;
    while(left < right){
        while(left < right && !queries_[queryIDList[left]]->isDelete_){
            if(queries_[queryIDList[left]]->isContain(*p))
                queries_[queryIDList[left]]->recvMesa();
            ++left;
        }
        while(left < right && queries_[queryIDList[right]]->isDelete_){
            if(--queries_[queryIDList[right]]->regiCounter_ == 0)
                freeQueryID_.push(queryIDList[right]);
            --right;
        }
        swap(queryIDList[left], queryIDList[right]);
    }
    if(queries_[queryIDList[left]]->isDelete_){
        if(--queries_[queryIDList[left]]->regiCounter_ == 0)
            freeQueryID_.push(queryIDList[left]);
        regCounter_ -= (queryIDList.size() - left);
        queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
    }else if(queries_[queryIDList[left]]->isContain(*p)){
        queries_[queryIDList[left]]->recvMesa();
    }
}

void RBVH::flush(){
    RBVHNode *node;
    while(!flushQ_.empty()){
        node = flushQ_.front();
        flushQ_.pop();
        node->inFlush_ = false;
        int b = node->buffer_;
        if(b == 0) continue;
        node->buffer_ = 0;
        auto &queryIDList = node->fullCover_;
        if(queryIDList.empty()) continue;
        int left = 0, right = queryIDList.size()-1;
        while(left < right){
            while(left < right && !queries_[queryIDList[left]]->isDelete_){
                queries_[queryIDList[left]]->recvMesa(b);
                ++left;
            }
            while(left < right && queries_[queryIDList[right]]->isDelete_){
                if(--queries_[queryIDList[right]]->regiCounter_ == 0)
                    freeQueryID_.push(queryIDList[right]);
                --right;
            }
            swap(queryIDList[left], queryIDList[right]);
        }
        if(queries_[queryIDList[left]]->isDelete_){
            if(--queries_[queryIDList[left]]->regiCounter_ == 0)
                freeQueryID_.push(queryIDList[left]);
            regCounter_ -= (queryIDList.size() - left);
            queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
        }else{
            queries_[queryIDList[left]]->recvMesa(b);
        }
    }
}

void RBVH::publishPoint(Point* point) {
    RBVHNode *node = root_;
    int subCode, height = 0;
    while (true) {
        if(node->children_ == nullptr){
            if(height < curMaxHeight_-1
               && node->partCover_.size() > max(static_cast<int>(curAliveQueryNum * ratioForSplit_), 64)){
                splitNode(node);
            }else{
                publishFull(node);
                if(!node->inFlush_ && node->buffer_ > 0){
                    node->inFlush_ = true;
                    flushQ_.push(node);
                }
                publishPart(node, point);
                break;
            }
        }
        if(node->children_ != nullptr){
            publishFull(node);
            if(!node->inFlush_ && node->buffer_ > 0){
                node->inFlush_ = true;
                flushQ_.push(node);
            }
            subCode = point->getSubCode(height);
            node = node->children_[subCode];
            ++height;
        }
    }
}

bool RBVH::incrementalConstruction(Point* point){
    RBVHNode *node = root_;
    int subCode, height = 0;
    bool tag = false;
    while (true) {
        if(node->children_ == nullptr){
            if(height < curMaxHeight_-1
               && node->partCover_.size() > max(static_cast<int>(curAliveQueryNum * ratioForSplit_), 64)){
                splitNode(node);
                tag = true;
            }
            else break;
        }
        if(node->children_ != nullptr){
            subCode = point->getSubCode(height);
            node = node->children_[subCode];
            ++height;
        }
    }
    return tag;
}

void RBVH::publishSynchronizedDelete(Point* point){
    RBVHNode *node = root_;
    int subCode, height = 0;
    while (true) {
        if(node->children_ == nullptr){
            publishFull(node);
            if(!node->inFlush_ && node->buffer_ > 0){
                node->inFlush_ = true;
                flushQ_.push(node);
            }
            publishPart(node, point);
            break;
        }
        if(node->children_ != nullptr){
            publishFull(node);
            if(!node->inFlush_ && node->buffer_ > 0){
                node->inFlush_ = true;
                flushQ_.push(node);
            }
            subCode = point->getSubCode(height);
            node = node->children_[subCode];
            ++height;
        }
    }
}

void RBVH::condenseTree(double curTime){
    bool drift = isDrift(curTime);
    if(drift){
//        RollBack2Base(root_, 0);
        rebuild();
        curMaxHeight_ = RBVH::maxH_;
        ratioForSplit_ = RBVH::minR_;
    }else if(ratioForSplit_ <= RBVH::maxR_){
        ratioForSplit_ += (RBVH::maxR_ - RBVH::minR_) / 4.0;
        cutHair(root_, 0);
    }else if(curMaxHeight_-1 >= RBVH::minH_){
        --curMaxHeight_;
        ratioForSplit_ = RBVH::minR_;
        cutHead(root_, 0);
    }else{
        throw std::runtime_error("cannot condense any more");
    }
}

void RBVH::rollBack2Base(RBVHNode *node, int height){
    static const int childNum = 1 << DIM;
    if(height < RBVH::minH_-1){
        if(node->children_ == nullptr) return;
        for(int idx = 0; idx < childNum; ++idx)
            rollBack2Base(node->children_[idx], height+1);
    }else if(node->children_ != nullptr){
        unordered_set<int> entryIDList;
        entryIDList.reserve(10000);
        queue<RBVHNode*> Q;
        for(int idx = 0; idx < childNum; ++idx)
            Q.push(node->children_[idx]);
        RBVHNode *tmp;
        while(!Q.empty()){
            tmp = Q.front();
            Q.pop();
            for(int entryID: tmp->fullCover_){
                --queries_[entryID]->regiCounter_;
                if(!queries_[entryID]->isDelete_)
                    entryIDList.insert(entryID);
                else if(queries_[entryID]->regiCounter_ == 0)
                    freeQueryID_.push(entryID);
            }
            for(int entryID: tmp->partCover_){
                --queries_[entryID]->regiCounter_;
                if(!queries_[entryID]->isDelete_)
                    entryIDList.insert(entryID);
                else if(queries_[entryID]->regiCounter_ == 0)
                    freeQueryID_.push(entryID);
            }
            if(tmp->children_ != nullptr){
                for(int idx = 0; idx < childNum; ++idx){
                    Q.push(tmp->children_[idx]);
                }
            }
            regCounter_ -= (tmp->fullCover_.size() + tmp->partCover_.size());
        }
        //node->partCover_.resize(entryIDList.size());
        node->partCover_.assign(entryIDList.begin(), entryIDList.end());
        regCounter_ += node->partCover_.size();
        for(auto& queryID: node->partCover_)
            ++queries_[queryID]->regiCounter_;
        for(int idx = 0; idx < childNum; ++idx)
            delete node->children_[idx];
        delete []node->children_;
        node->children_ = nullptr;
    }
}

bool RBVH::cutHair(RBVHNode *node, int height){
    static const int childNum = 1 << DIM;
    if(node->children_ == nullptr) return true;
    bool isValid = true;
    for(int idx = 0; idx < childNum; ++idx){
        if(node->children_[idx]->children_ != nullptr)
            isValid &= cutHair(node->children_[idx], height+1);
    }
    if(!isValid) return false;
    RBVHNode *tmp;
    for(int idx = 0; idx < childNum; ++idx){
        tmp = node->children_[idx];
        if(tmp->fullCover_.size() + tmp->partCover_.size() > static_cast<size_t>(curAliveQueryNum * ratioForSplit_))
            return false;
    }
    unordered_map<int, int> entryIDList;
    entryIDList.reserve(10000);
    for(int idx = 0; idx < childNum; ++idx){
        tmp = node->children_[idx];
        if(!tmp->fullCover_.empty()){
            vector<int> &queryIDList = tmp->fullCover_;
            int left = 0, right = queryIDList.size()-1;
            while(left < right){
                while(left < right && !queries_[queryIDList[left]]->isDelete_){
                    entryIDList[queryIDList[left]]++;
                    ++left;
                }
                while(left < right && queries_[queryIDList[right]]->isDelete_){
                    if(--queries_[queryIDList[right]]->regiCounter_ == 0)
                        freeQueryID_.push(queryIDList[right]);
                    --right;
                }
                swap(queryIDList[left], queryIDList[right]);
            }
            if(queries_[queryIDList[left]]->isDelete_){
                if(--queries_[queryIDList[left]]->regiCounter_ == 0)
                    freeQueryID_.push(queryIDList[left]);
                regCounter_ -= (queryIDList.size() - left);
                queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
            }else{
                entryIDList[queryIDList[left]]++;
            }
        }
        if(!tmp->partCover_.empty()){
            vector<int> &queryIDList = tmp->partCover_;
            int left = 0, right = queryIDList.size()-1;
            while(left < right){
                while(left < right && !queries_[queryIDList[left]]->isDelete_){
                    entryIDList[queryIDList[left]]++;
                    ++left;
                }
                while(left < right && queries_[queryIDList[right]]->isDelete_){
                    if(--queries_[queryIDList[right]]->regiCounter_ == 0)
                        freeQueryID_.push(queryIDList[right]);
                    --right;
                }
                swap(queryIDList[left], queryIDList[right]);
            }
            if(queries_[queryIDList[left]]->isDelete_){
                if(--queries_[queryIDList[left]]->regiCounter_ == 0)
                    freeQueryID_.push(queryIDList[left]);
                regCounter_ -= (queryIDList.size() - left);
                queryIDList.erase(queryIDList.begin()+left, queryIDList.end());
            }else{
                entryIDList[queryIDList[left]]++;
            }
        }
        if(entryIDList.size() > static_cast<size_t>(curAliveQueryNum * ratioForSplit_)) return false;
    }
    node->partCover_.resize(entryIDList.size());
    int entryIDX = 0;
    for(auto &IDNumPair: entryIDList){
        node->partCover_[entryIDX++] = IDNumPair.first;
        queries_[IDNumPair.first]->regiCounter_ -= (IDNumPair.second - 1);
        regCounter_ -= IDNumPair.second;
    }
    regCounter_ += node->partCover_.size();
    for(int idx = 0; idx < childNum; ++idx)
        delete node->children_[idx];
    delete []node->children_;
    node->children_ = nullptr;
    return true;
}

void RBVH::cutHead(RBVHNode *node, int height){
    static const int childNum = 1 << DIM;
    if(node->children_ == nullptr) return;
    if(height < curMaxHeight_-1){
        for(int idx = 0; idx < childNum; ++idx)
            cutHead(node->children_[idx], height+1);
    }else{
        unordered_set<int> entryIDList;
        entryIDList.reserve(10000);
        RBVHNode *tmp;
        for(int idx = 0; idx < childNum; ++idx){
            tmp = node->children_[idx];
            for(int entryID: tmp->fullCover_){
                --queries_[entryID]->regiCounter_;
                if(!queries_[entryID]->isDelete_)
                    entryIDList.insert(entryID);
                else if(queries_[entryID]->regiCounter_ == 0)
                    freeQueryID_.push(entryID);
            }
            for(int entryID: tmp->partCover_){
                --queries_[entryID]->regiCounter_;
                if(!queries_[entryID]->isDelete_)
                    entryIDList.insert(entryID);
                else if(queries_[entryID]->regiCounter_ == 0)
                    freeQueryID_.push(entryID);
            }
            regCounter_ -= (tmp->fullCover_.size() + tmp->partCover_.size());
            delete tmp;
        }
        node->partCover_.resize(entryIDList.size());
        node->partCover_.assign(entryIDList.begin(), entryIDList.end());
        for(auto& queryID: node->partCover_){
            ++queries_[queryID]->regiCounter_;
        }
        regCounter_ += node->partCover_.size();
        delete []node->children_;
        node->children_ = nullptr;
    }
}

void RBVH::rebuild(){
    static const int childNum = 1 << DIM;
    if(root_->children_ != nullptr){
        for(int idx = 0; idx < childNum; ++idx){
            delete root_->children_[idx];
        }
    }
    delete []root_->children_;
    root_->children_ = nullptr;
    regCounter_ = 0;
    int entryNum = queries_.size();
    for(int idx = 0; idx < entryNum; ++idx){
        if(!queries_[idx]->isDelete_){
            root_->partCover_.push_back(idx);
            queries_[idx]->regiCounter_ = 1;
            ++regCounter_;
        }else if(queries_[idx]->regiCounter_ > 0){
            freeQueryID_.push(idx);
            queries_[idx]->regiCounter_ = 0;
        }
    }
}

void RBVH::updateSTList(const Point *p, double curTime){
    for(int dim = 0; dim < DIM; ++dim){
        int idx = (int)((p->cord_[dim] - root_->low_[dim]) / cellLength_[dim]);
        STCell &cell = curSTVec_[dim][idx];
        double dt = curTime - cell.lastTime_;
        if (dt > 0) {
            cell.intensity_ *= std::exp(-RBVH::lambda_ * dt);
        }
        cell.intensity_ += 1.0;
        cell.lastTime_ = curTime;
    }
}

bool RBVH::isDrift(double curTime) {
    double dt, sc, lsc, similarity = 0.0;
    for(int dim = 0; dim < DIM; ++dim){
        double dot = 0.0, na = 0.0, nb = 0.0;
        for(int idx = 0; idx < 256; ++idx){
            dt = curTime - curSTVec_[dim][idx].lastTime_;
            if(dt > 0){
                sc = curSTVec_[dim][idx].intensity_ * std::exp(-RBVH::lambda_ * dt);
                sc = 1.0 - std::exp(-sc / RBVH::kappa_);
            }else
                sc = 0;

            lsc = lastSTVec_[dim][idx].intensity_;
            dot += lsc * sc;
            na  += sc * sc;
            nb  += lsc * lsc;

            lastSTVec_[dim][idx].intensity_ = sc;
            curSTVec_[dim][idx].intensity_ = 0.0;
            curSTVec_[dim][idx].lastTime_ = curTime;
        }
        if (na > 1e-12 && nb > 1e-12)
            similarity += dot / (std::sqrt(na) * std::sqrt(nb)) / static_cast<double>(DIM);
    }
    if(similarity < RBVH::tau_){
        cout << "Drifted! Similarity: " << similarity << endl << endl;
        return true;
    }
    cout << "Stable! Similarity: " << similarity << endl << endl;
    return false;
}

float RBVH::getFlushInterval(){
    if(flushMap_.empty()) return 1e5;
    return flushMap_.begin()->first;
}

bool RBVH::checkConsistency() const {
    const int childNum = 1 << DIM;
    size_t regCounterInIndex = 0;
    size_t regCounterQuerySum = 0;
    queue<RBVHNode*> Q;
    RBVHNode *tmp;
    Q.push(root_);
    while(!Q.empty()){
        tmp = Q.front();
        Q.pop();
        regCounterInIndex += tmp->fullCover_.size() + tmp->partCover_.size();
        if(tmp->children_ != nullptr)
            for(int idx = 0; idx < childNum; ++idx)
                Q.push(tmp->children_[idx]);
    }
    for(auto& query: queries_)
        regCounterQuerySum += query->regiCounter_;

    if(regCounter_ != regCounterQuerySum ||
       regCounter_ != regCounterInIndex) {
        std::cerr << "[R-BVH Consistency Error]\n"
                  << " regCounter_          = " << regCounter_ << "\n"
                  << " regCounterInIndex    = " << regCounterInIndex << "\n"
                  << " regCounterQuerySum   = " << regCounterQuerySum << "\n";
        return false;
    }
    assert(regCounter_ == regCounterQuerySum && regCounter_ == regCounterInIndex);
    return true;
}

