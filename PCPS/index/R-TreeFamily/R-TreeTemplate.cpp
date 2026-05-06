#include "R-TreeTemplate.h"

int RTreeNode::maxEntry_ = 50;
int RTreeNode::minEntry_ = 20;

bool RTreeNode::addEntry(int entryID) {
    entries_.push_back(entryID);
    return ++entryNum_ > RTreeNode::maxEntry_;
}

bool RTreeNode::removeEntry(int entryID) {
    auto it = find(entries_.begin(), entries_.end(), entryID);
    if(it != entries_.end())
        entries_.erase(it);
    return --entryNum_ < RTreeNode::minEntry_;
}

void RTreeNode::copyEntries(const vector<int>& entries) {
    if(!entries_.empty())
        entries_.clear();
    entries_.assign(entries.begin(), entries.end());
    entryNum_ = static_cast<int>(entries.size());
}

void RTreeNode::setBound(const Rectangle& rect){
//    assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
    low_.assign(rect.low_.begin(), rect.low_.end());
    top_.assign(rect.top_.begin(), rect.top_.end());
    for(int dim = 0; dim < DIM; ++dim)
        center_[dim] = (low_[dim] + top_[dim]) * 0.5;
}

void RTreeNode::setBound(const vector<double>& low, const vector<double>& top){
//    assert(low.size() == DIM && top.size() == DIM);
    low_.assign(low.begin(), low.end());
    top_.assign(top.begin(), top.end());
    for(int dim = 0; dim < DIM; ++dim)
        center_[dim] = (low[dim] + top[dim]) * 0.5;
}


RTreeNode* RTreeTemplate::createNode() {
    if(freeNodeID_.empty()){
        RTreeNode *node = new RTreeNode();
        node->nodeID_ = static_cast<int>(treeNodes_.size());
        treeNodes_.push_back(node);
        return node;
    }else{
        int nodeID = freeNodeID_.front();
        freeNodeID_.pop();
        treeNodes_[nodeID]->entries_.clear();
        treeNodes_[nodeID]->father_ = -1;
        treeNodes_[nodeID]->height_ = 0;
        treeNodes_[nodeID]->entryNum_ = 0;
        return treeNodes_[nodeID];
    }
}

Rectangle RTreeTemplate::mergeRange(RTreeNode* node, const int startIDX, const int endIDX){
    vector<int>& entryIDList = node->entries_;
    if(node->height_ == 0){
        Rectangle rectangle(*queries_[entryIDList[startIDX]]);
        for (int idx = startIDX+1; idx < endIDX; ++idx) {
            rectangle.merge(*queries_[entryIDList[idx]]);
        }
        return rectangle;
    }else{
        Rectangle rectangle(*treeNodes_[entryIDList[startIDX]]);
        for (int idx = startIDX+1; idx < endIDX; ++idx) {
            rectangle.merge(*treeNodes_[entryIDList[idx]]);
        }
        return rectangle;
    }
}

void RTreeTemplate::registerCRQuery(CRQuery* query) {
    if(!query->isValid()){
        throw std::runtime_error("invalid query range");
    }
    int queryID = createQuery(query);

    ++regCounter_;
    queries_[queryID]->regiCounter_ = 1;

    // if tree is empty, create a root node and set the bound
    if(root_ < 0){
        RTreeNode *newRoot = createNode();
        newRoot->setBound(query->low_, query->top_);
        newRoot->addEntry(queryID);
        height_ = 1;
        root_ = 0;
        return;
    }

    // if tree is not empty, traverse a path to a specific leaf node using function chooseSubtree
    RTreeNode *node = treeNodes_[root_];
    node->merge(*query);

    // choose a child node of the internal node to insert and update the bound
    while(node->height_ > 0){
        node = chooseSubtree(queries_[queryID], node);
        node->merge(*query);
    }
    bool isOverflowed = node->addEntry(queryID);

    // split the node if overflowed and callback along the traverse path to update the tree
    while(isOverflowed){
        node = splitNode(node);
        isOverflowed = node->entries_.size() > RTreeNode::maxEntry_;
    }
}

RTreeNode* RTreeTemplate::splitNode(RTreeNode* node){
    vector<int> group1, group2;
    Rectangle rect1, rect2;
    partition(node, group1, group2, rect1, rect2);

    RTreeNode *sibling = createNode();
    sibling->height_ = node->height_;

    node->copyEntries(group1);
    node->setBound(rect1);
    sibling->copyEntries(group2);
    sibling->setBound(rect2);

    if(sibling->height_ > 0){
        for(int &entryID : sibling->entries_){
            treeNodes_[entryID]->father_ = sibling->nodeID_;
        }
        for(int &entryID : node->entries_){
            treeNodes_[entryID]->father_ = node->nodeID_;
        }
    }
    if(node->father_ >= 0){
        treeNodes_[node->father_]->addEntry(sibling->nodeID_);
        treeNodes_[node->father_]->merge(rect2);
        sibling->father_ = node->father_;
    } else {
        RTreeNode *newRoot = createNode();
        newRoot->addEntry(node->nodeID_);
        newRoot->addEntry(sibling->nodeID_);
        newRoot->setBound(rect1);
        newRoot->merge(rect2);
        newRoot->height_ = height_++;

        root_ = newRoot->nodeID_;
        node->father_ = newRoot->nodeID_;
        sibling->father_ = newRoot->nodeID_;
    }
    return treeNodes_[node->father_];
}

void RTreeTemplate::cancelCRQuery(CRQuery* query) {
    int qid = query->queryID_;
    if(qid < 0 || qid >= queries_.size()){
        cout << "queryID: " << query->queryID_ << "; query list size: " << queries_.size() << endl;
        throw std::runtime_error("queryID out of range.");
    }
    if(queries_[qid]->isDelete_) {
        cout << "queryID: " << query->queryID_ << endl;
        throw std::runtime_error("query already deleted.");
    }
    if(!query->isEqual(*queries_[qid])){
        throw std::runtime_error("unequal target query");
    }

    queries_[qid]->isDelete_ = true;
    queries_[qid]->regiCounter_ = 0;
    result_.addResult(queries_[qid]->mesaCounter_);
    freeQueryID_.push(qid);
    --regCounter_;

    RTreeNode *node = findLeaf(*query);
    if(node->nodeID_ == root_) {
        node->setBound(mergeRange(node, 0, node->entryNum_));
        return;
    }
    vector<ReInsertItem> reinsertList;
    condenseTree(node, reinsertList);
    reinsert(reinsertList);
}

RTreeNode* RTreeTemplate::findLeaf(CRQuery& query) {
    queue<int> nodes;
    RTreeNode *node;
    nodes.push(root_);
    bool tag = true;
    while(!nodes.empty() && tag){
        node = treeNodes_[nodes.front()];
        nodes.pop();
        if(node->height_ == 0){
            for(const int queryID: node->entries_)
                if(queries_[queryID]->isEqual(query)){
                    tag = false;
                    node->removeEntry(queryID);
                    break;
                }
        }else{
            for(const int nodeID: node->entries_)
                if(treeNodes_[nodeID]->isContain(query))
                    nodes.push(nodeID);
        }
    }
    return node;
}

void RTreeTemplate::condenseTree(RTreeNode *node, vector<ReInsertItem> &reinsertList){
//    assert(node->height_ == 0);
    while (node->nodeID_ != root_) {
        RTreeNode* father = treeNodes_[node->father_];
        if (node->entryNum_ < RTreeNode::minEntry_) {
            father->removeEntry(node->nodeID_);
            freeNodeID_.push(node->nodeID_);

            if (node->height_ == 0) {
                for (const int queryID: node->entries_)
                    reinsertList.push_back({EntryType::Query, queryID});
            } else {
                for (const int nodeID : node->entries_)
                    reinsertList.push_back({EntryType::Node, nodeID});
            }
        } else {
            node->setBound(mergeRange(node, 0, node->entryNum_));
        }
        node = father;
    }
    // now node is the root
    node->setBound(mergeRange(node, 0, node->entryNum_));
}

void RTreeTemplate::reinsert(vector<ReInsertItem> &reinsertList){
    for(auto &item: reinsertList){
        RTreeNode *node = treeNodes_[root_];
        bool isOverflowed;
        if(item.entryType_ == EntryType::Query){
            CRQuery* query = queries_[item.itemID_];
            node->merge(*query);
            // choose a child node of the internal node to insert and update the bound
            while(node->height_ > 0){
                node = chooseSubtree(query, node);
                node->merge(*query);
            }
            isOverflowed = node->addEntry(query->queryID_);
        }else{
            RTreeNode *subtree = treeNodes_[item.itemID_];
            node->merge(*subtree);
            // choose a child node of the internal node to insert and update the bound
            while(node->height_ > subtree->height_+1){
                node = chooseSubtree(subtree, node);
                node->merge(*subtree);
            }
            isOverflowed = node->addEntry(subtree->nodeID_);
            subtree->father_ = node->nodeID_;
        }
        // split the node if overflowed and callback along the traverse path to update the tree
        while(isOverflowed){
            node = splitNode(node);
            isOverflowed = node->entries_.size() > RTreeNode::maxEntry_;
        }
    }
    RTreeNode *tmp = treeNodes_[root_];
    if(tmp->entryNum_ == 1){
        --height_;
        freeNodeID_.push(root_);
        root_ = tmp->entries_[0];
    }
}

void RTreeTemplate::publishPoint(Point* point) {
    queue<int> queryQue;
    RTreeNode *node;
    queryQue.push(root_);
    while(!queryQue.empty()){
        node = treeNodes_[queryQue.front()];
        queryQue.pop();
        if(node->height_ == 0){
            for(auto queryID: node->entries_)
                if(queries_[queryID]->isContain(*point))
                    queries_[queryID]->recvMesa();
        }else{
            for(auto nodeID: node->entries_)
                if(treeNodes_[nodeID]->isContain(*point))
                    queryQue.push(nodeID);
        }
    }
}

bool RTreeTemplate::checkConsistency() const{
    long long int regCounterInIndex = 0;
    long long int regCounterQuerySum = 0;

    queue<RTreeNode*> Q;
    RTreeNode *tmp;
    Q.push(treeNodes_[root_]);
    while(!Q.empty()){
        tmp = Q.front();
        Q.pop();
//        assert(tmp->entryNum_ == tmp->entries_.size());
        if(tmp->height_ == 0){
            regCounterInIndex += tmp->entryNum_;
        }
        else{
            for(int nodeID: tmp->entries_)
                Q.push(treeNodes_[nodeID]);
        }
    }
    for(auto& query: queries_)
        regCounterQuerySum += query->regiCounter_;

    if(regCounter_ != regCounterQuerySum ||
       regCounter_ != regCounterInIndex) {
        std::cerr << "[R-Tree Family Index Consistency Error]\n"
                  << " regCounter_          = " << regCounter_ << "\n"
                  << " regCounterInIndex    = " << regCounterInIndex << "\n"
                  << " regCounterQuerySum   = " << regCounterQuerySum << "\n";
        return false;
    }
    assert(regCounter_ == regCounterQuerySum && regCounter_ == regCounterInIndex);
    return true;
}