#include "R-Tree.h"

int TreeNode::maxEntry_ = 50;
int TreeNode::minEntry_ = 20;

double splitByOverlap(const Rectangle& rect1, const Rectangle& rect2){
    auto overlap = rect1.getOverlap(rect2);
    if(!overlap.isValid()){
        return 0;
    }
    return overlap.getVolume();
}

double splitByVolume(const Rectangle& rect1, const Rectangle& rect2){
    return rect1.getVolume() + rect2.getVolume();
}

bool TreeNode::addEntry(int entryID) {
    entries_.push_back(entryID);
    return ++entryNum_ > TreeNode::maxEntry_;
}

void TreeNode::copyEntries(const vector<int>& entries) {
    if(!entries_.empty())
        entries_.clear();
    entries_.assign(entries.begin(), entries.end());
    entryNum_ = static_cast<int>(entries.size());
}

TreeNode* RTree::createNode() {
    auto *node = new TreeNode();
    node->rectID_ = static_cast<int>(treeNodes_.size());
    treeNodes_.push_back(node);
    return node;
}

Rectangle* RTree::createRect(const vector<double>& low, const vector<double>& top) {
    auto* rect = new Rectangle(low, top);
    rect->rectID_ = static_cast<int>(objects_.size());
    objects_.push_back(rect);
    return rect;
}

void RTree::copyTree(RTree* tree) {
    for (auto & object : objects_) {
        delete object;
    }
    for (auto & treeNode : treeNodes_) {
        delete treeNode;
    }
    objects_.resize(tree->objects_.size());
    treeNodes_.resize(tree->treeNodes_.size());
    for (int idx = 0; idx < objects_.size(); idx++) {
        objects_[idx] = new Rectangle(*tree->objects_[idx]);
        objects_[idx]->rectID_ = tree->objects_[idx]->rectID_;
        assert(objects_[idx]->rectID_ == idx);
    }
    for (int idx = 0; idx < treeNodes_.size(); idx++) {
        treeNodes_[idx] = new TreeNode(tree->treeNodes_[idx]);
        assert(treeNodes_[idx]->rectID_ == idx);
    }
    root_ = tree->root_;
    height_ = tree->height_;
    resNum_ = 0;
}

void RTree::clearTree(){
    for (auto & object : objects_) {
        delete object;
    }
    for (auto & treeNode : treeNodes_) {
        delete treeNode;
    }
    objects_.clear();
    treeNodes_.clear();
    root_ = -1;
    height_ = 0;
    resNum_ = 0;
}

Rectangle RTree::mergeRange(TreeNode* node, int startIDX, int endIDX){
    vector<int>& entryIDList = node->entries_;
    if(node->height_ == 0){
        Rectangle rectangle(*objects_[entryIDList[startIDX]]);
        for (int idx = startIDX+1; idx < endIDX; ++idx) {
            rectangle.merge(*objects_[entryIDList[idx]]);
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

void RTree::insertRect(Rectangle& rectangle){
    auto rect = createRect(rectangle.low_, rectangle.top_);

    // if tree is empty, create a root node and set the bound
    if(root_ < 0){
        auto* newRoot = createNode();
        newRoot->setBound(rectangle);
        newRoot->addEntry(rect->rectID_);
        height_ = 1;
        root_ = 0;
        return;
    }

    // if tree is not empty, traverse a path to a specific leaf node using function chooseSubtree
    TreeNode *node = treeNodes_[root_];
    node->merge(*rect);

    // choose a child node of the internal node to insert and update the bound
    while(node->height_ > 0){
        node = chooseSubtree(rect, node);
        node->merge(*rect);
    }
    bool isOverflowed = node->addEntry(rect->rectID_);

    // split the node if overflowed and callback along the traverse path to update the tree
    while(isOverflowed){
        node = splitNode(node);
        isOverflowed = node->entries_.size() > TreeNode::maxEntry_;
    }
}

TreeNode* RTree::chooseSubtree(Rectangle* rect, TreeNode* node){
    TreeNode* chooseNode = nullptr;
    auto minVolumeIncrease = DBL_MAX;
    for (auto& nodeID: node->entries_) {
        TreeNode* subtree = treeNodes_[nodeID];
        Rectangle newRect = subtree->getMerge(*rect);
        double volumeIncrease = newRect.getVolume() - subtree->getVolume();
        if (volumeIncrease < minVolumeIncrease) {
            //choose the subtree with the smaller area increase
            minVolumeIncrease = volumeIncrease;
            chooseNode = subtree;
        } else if (volumeIncrease == minVolumeIncrease) {
            //break the tie by favoring the smaller MBR
            if (chooseNode->getVolume() > subtree->getVolume())
                chooseNode = subtree;
        }
    }
    return chooseNode;
}

TreeNode* RTree::splitNode(TreeNode* node){
    vector<int> group1, group2;
    Rectangle rect1, rect2;
    partition(node, group1, group2, rect1, rect2);

    TreeNode *sibling = createNode();
    sibling->height_ = node->height_;

    node->copyEntries(group1);
    node->setBound(rect1);
    sibling->copyEntries(group2);
    sibling->setBound(rect2);

    if(sibling->height_ > 0){
        for(int &entryID : sibling->entries_){
            treeNodes_[entryID]->father_ = sibling->rectID_;
        }
        for(int &entryID : node->entries_){
            treeNodes_[entryID]->father_ = node->rectID_;
        }
    }
    if(node->father_ >= 0){
        treeNodes_[node->father_]->addEntry(sibling->rectID_);
        treeNodes_[node->father_]->merge(rect2);
        sibling->father_ = node->father_;
    } else {
        TreeNode *newRoot = createNode();
        newRoot->addEntry(node->rectID_);
        newRoot->addEntry(sibling->rectID_);
        newRoot->setBound(rect1);
        newRoot->merge(rect2);
        newRoot->height_ = height_++;

        root_ = newRoot->rectID_;
        node->father_ = newRoot->rectID_;
        sibling->father_ = newRoot->rectID_;
    }
    return treeNodes_[node->father_];
}

void RTree::partition(TreeNode* node, vector<int>& group1, vector<int>& group2, Rectangle& rect1, Rectangle& rect2){
    vector<int>& entryIDList = node->entries_;
    auto minOverlap = DBL_MAX, minVolume = DBL_MAX;

    //choose the split with the minimum overlap, break the tie by preferring the split with smaller total area
    for(int dim = 0; dim < DIM; ++dim){
        for(int splitDis : {0, 1}) {
            if (splitDis == 0) { // sort by lower bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                    if (node->height_ == 0) {
                        return objects_[idx1]->low_[dim] < objects_[idx2]->low_[dim];
                    } else {
                        return treeNodes_[idx1]->low_[dim] < treeNodes_[idx2]->low_[dim];
                    }
                });
            } else { // sort by upper bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                    if (node->height_ == 0) {
                        return objects_[idx1]->top_[dim] < objects_[idx2]->top_[dim];
                    } else {
                        return treeNodes_[idx1]->top_[dim] < treeNodes_[idx2]->top_[dim];
                    }
                });
            }

            Rectangle prefixRect = mergeRange(node, 0, TreeNode::minEntry_ - 1);
            Rectangle suffixRect = mergeRange(node, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1, node->entryNum_);
            for (int candidate = TreeNode::minEntry_ - 1; candidate < TreeNode::maxEntry_ - TreeNode::minEntry_ + 1; candidate++) {
                if (node->height_ == 0)
                    prefixRect.merge(*objects_[entryIDList[candidate]]);
                else
                    prefixRect.merge(*treeNodes_[entryIDList[candidate]]);
                Rectangle remainingRect(suffixRect);
                remainingRect.merge(mergeRange(node, candidate + 1, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1));
                double overlap = splitByOverlap(prefixRect, remainingRect);
                double volume = splitByVolume(prefixRect, remainingRect);
                if (overlap < minOverlap || (overlap == minOverlap && volume < minVolume)) {
                    minOverlap = overlap;
                    minVolume = volume;
                    group1.assign(entryIDList.begin(), entryIDList.begin() + candidate + 1);
                    group2.assign(entryIDList.begin() + candidate + 1, entryIDList.end());
                    rect1.setBound(prefixRect);
                    rect2.setBound(remainingRect);
                }
            }
        }
    }
}

int RTree::stabbingQuery(Point& point) {
    int nodeAccess = 0;
    int resultCnt = 0;
    queue<TreeNode*> Q;
    Q.push(treeNodes_[root_]);
    TreeNode* node = treeNodes_[root_];
    if (!node->isContain(point)) {
        resNum_ = 0;
        return 0;
    }
    while (!Q.empty()) {
        node = Q.front();
        Q.pop();
        ++nodeAccess;
        if (node->height_ == 0) {
            for (int idx = 0; idx < node->entryNum_; idx++) {
                Rectangle* rect = objects_[node->entries_[idx]];
                if (rect->isContain(point))
                    ++resultCnt;
            }
        } else {
            for (int idx = 0; idx < node->entryNum_; idx++) {
                TreeNode* tmp = treeNodes_[node->entries_[idx]];
                if (tmp->isContain(point))
                    Q.push(tmp);
            }
        }
    }
    resNum_ = resultCnt;
    return nodeAccess;
}

int RTree::rangeQuery(Rectangle &range) {
    int nodeAccess = 0;
    int resultCnt = 0;
    queue<TreeNode*> Q;
    Q.push(treeNodes_[root_]);
    TreeNode* node = treeNodes_[root_];
    if (!node->isOverlap(range)) {
        resNum_ = 0;
        return 0;
    }
    while (!Q.empty()) {
        node = Q.front();
        Q.pop();
        ++nodeAccess;
        if (node->height_ == 0) {
            for (int idx = 0; idx < node->entryNum_; idx++) {
                Rectangle* rect = objects_[node->entries_[idx]];
                if (rect->isOverlap(range))
                    ++resultCnt;
            }
        } else {
            for (int idx = 0; idx < node->entryNum_; idx++) {
                TreeNode* tmp = treeNodes_[node->entries_[idx]];
                if (tmp->isOverlap(range))
                    Q.push(tmp);
            }
        }
    }
    resNum_ = resultCnt;
    return nodeAccess;
}

double RTree::accessRateStabbing(Point &point) {
    if(height_ == 0)
        throw std::runtime_error("empty tree");
    int accessNode = stabbingQuery(point);
    return 1.0 * accessNode / height_;
}

double RTree::accessRateRange(Rectangle &range) {
    if(height_ == 0)
        throw std::runtime_error("empty tree");
    int accessNode = rangeQuery(range);
    return 1.0 * accessNode / height_;
}

bool RTree::checkConsistency() {
    queue<TreeNode*> Q;
    Q.push(treeNodes_[root_]);
    int rectNum = 0, nodeNum = 0;
    while (!Q.empty()) {
        TreeNode* node = Q.front();
        Q.pop();
        nodeNum++;
        if(node->entries_.size() != node->entryNum_) {
            cout << "node->entryNum_ != node->entries_.size()" << endl;
            return false;
        }
        Rectangle children = mergeRange(node, 0, node->entryNum_);
        if(!node->isEqual(children)){
            cout << "node MBR != children MBR" << endl;
            return false;
        }
        if(node->height_ > 0){
            for (int idx = 0; idx < node->entryNum_; idx++) {
                TreeNode* tmp = treeNodes_[node->entries_[idx]];
                Q.push(tmp);
            }
        }else{
            rectNum += node->entryNum_;
        }
    }
    if(rectNum != objects_.size()){
        cout << "rect num is not correct" << endl;
        cout << "rect num: " << rectNum << ", objects_.size(): " << objects_.size() << endl;
        return false;
    }
    if(nodeNum != treeNodes_.size()){
        cout << "node num is not correct" << endl;
        cout << "node num: " << nodeNum << ", treeNodes_.size(): " << treeNodes_.size() << endl;
        return false;
    }
    return true;
}
