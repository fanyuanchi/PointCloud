#include "RS-Tree.h"

double RSTree::ratioForReinsert_ = 0.3;

void RSTree::registerCRQuery(CRQuery* query) {
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
    // if the leaf node is overflowed, invoke forcedReinsert to reorganize entries in R*-Tree
    if(isOverflowed){
        if(node->nodeID_ == root_){
            // root split
            splitNode(node);
        }else{
            unordered_set<int> curReinsert;
            forcedReinsert(node, curReinsert);
        }
    }
}

RTreeNode* RSTree::chooseSubtree(Rectangle* rect, RTreeNode* node){
    vector<RTreeNode*> COV;
    int chosenID = -1;
    for(int &nodeID : node->entries_){
        if(treeNodes_[nodeID]->isContain(*rect)){
            COV.push_back(treeNodes_[nodeID]);
        }
    }
    if(!COV.empty()){
        auto minVolume = DBL_MAX;
        for(auto tmp: COV){
            double volume = tmp->getVolume();
            if(volume < minVolume){
                minVolume = volume;
                chosenID = tmp->nodeID_;
            }
        }
    }else if(node->height_ == 1){
        // if target node is leaf, choose the child node (leaf) with minimum overlap enlargement with other siblings if inserted
        double deltaOvlp, minDeltaOvlp = DBL_MAX, difference, minEnlargement = DBL_MAX;
        for(int &idx1 : node->entries_){
            double oldOvlp = 0.0, newOvlp = 0.0;
            difference = treeNodes_[idx1]->getMerge(*rect).getVolume() - treeNodes_[idx1]->getVolume();
            for(int &idx2 : node->entries_){
                if(idx1 == idx2) continue;
                oldOvlp += treeNodes_[idx1]->getOverlap(*treeNodes_[idx2]).getVolume();
                newOvlp += treeNodes_[idx1]->getMerge(*rect).getOverlap(*treeNodes_[idx2]).getVolume();
                deltaOvlp = newOvlp - oldOvlp;
            }
            if(deltaOvlp < minDeltaOvlp){
                minEnlargement = difference;
                minDeltaOvlp = deltaOvlp;
                chosenID = idx1;
            }else if (deltaOvlp == minDeltaOvlp){
                if(difference < minEnlargement){
                    minEnlargement = difference;
                    chosenID = idx1;
                }
            }
        }
    }else{
        // if target node is not leaf, choose the child node (subtree) with minimum volume enlargement with other siblings if inserted
        double difference, minEnlargement = DBL_MAX;
        for(int &nodeID : node->entries_){
            difference = treeNodes_[nodeID]->getMerge(*rect).getVolume() - treeNodes_[nodeID]->getVolume();
            if(difference < minEnlargement){
                minEnlargement = difference;
                chosenID = nodeID;
            }
        }
    }
    return treeNodes_[chosenID];
}

void RSTree::forcedReinsert(RTreeNode *node, unordered_set<int>& curReinsert) {
//    assert(!curReinsert.contains(node->height_));
    curReinsert.insert(node->height_);

    EntryType entryType = node->height_ == 0 ? EntryType::Query : EntryType::Node;
    vector<int>& entryIDList = node->entries_;
    sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int idx2)->bool{
        if(entryType == EntryType::Query){
            return queries_[idx1]->getDistance(*node) > queries_[idx2]->getDistance(*node);
        }else{
            return treeNodes_[idx1]->getDistance(*node) > treeNodes_[idx2]->getDistance(*node);
        }
    });
    vector<ReInsertItem> reinsertList;
    int reinsertNum = ceil(RTreeNode::maxEntry_ * RSTree::ratioForReinsert_);
    reinsertList.reserve(reinsertNum);
    for(int idx = 0; idx < reinsertNum; ++idx){
        reinsertList.push_back({entryType, entryIDList[idx]});
    }
    entryIDList.erase(entryIDList.begin(), entryIDList.begin() + reinsertNum);
    node->entryNum_ -= reinsertNum;
    // update the bound from the reinserted node to the root
    while(node->nodeID_ != root_){
        node->setBound(mergeRange(node,0, node->entryNum_));
        node = treeNodes_[node->father_];
    }
    // now node is the root
    node->setBound(mergeRange(node,0, node->entryNum_));

    for(auto& item: reinsertList){
        reinsertEntry(item, curReinsert);
    }
}

void RSTree::reinsertEntry(ReInsertItem item, unordered_set<int>& curReinsert){
    bool isOverflowed;
    RTreeNode *node = treeNodes_[root_];
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
        if(curReinsert.contains(node->height_)){
            node = splitNode(node);
            isOverflowed = node->entries_.size() > RTreeNode::maxEntry_;
        }else{
            forcedReinsert(node, curReinsert);
            break;
        }
    }
}

void RSTree::partition(RTreeNode* node,
                       vector<int>& group1, vector<int>& group2, Rectangle& rect1, Rectangle& rect2) {
    EntryType entryType = node->height_ == 0 ? EntryType::Query : EntryType::Node;
    vector<int>& entryIDList = node->entries_;
    int splitDim;
    double sumPerimeter, minPerimeter = DBL_MAX;
    // choose split axis
    for(int dim = 0; dim < DIM; ++dim){
        sumPerimeter = 0.0;
        for(int splitDis : {0, 1}){
            if(splitDis == 0){ // sort by lower bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
                    if(entryType == EntryType::Query)
                        return queries_[idx1]->low_[dim] < queries_[idx2]->low_[dim];
                    else
                        return treeNodes_[idx1]->low_[dim] < treeNodes_[idx2]->low_[dim];
                });
            }else{ // sort by upper bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
                    if(entryType == EntryType::Query)
                        return queries_[idx1]->top_[dim] < queries_[idx2]->top_[dim];
                    else
                        return treeNodes_[idx1]->top_[dim] < treeNodes_[idx2]->top_[dim];
                });
            }
            Rectangle recPrefix = mergeRange(node,0, RTreeNode::minEntry_-1);
            Rectangle recSuffix = mergeRange(node,RTreeNode::maxEntry_-RTreeNode::minEntry_+1, node->entryNum_);
            for(int candidate = RTreeNode::minEntry_-1; candidate < RTreeNode::maxEntry_-RTreeNode::minEntry_+1; ++candidate){
                if(entryType == EntryType::Query)
                    recPrefix.merge(*queries_[entryIDList[candidate]]);
                else
                    recPrefix.merge(*treeNodes_[entryIDList[candidate]]);
                Rectangle recRemaining(recSuffix);
                recRemaining.merge(mergeRange(node,candidate+1, RTreeNode::maxEntry_-RTreeNode::minEntry_+1));
                sumPerimeter += recPrefix.getPerimeter() + recRemaining.getPerimeter();
            }
        }
        if(sumPerimeter < minPerimeter){
            minPerimeter = sumPerimeter;
            splitDim = dim;
        }
    }
    // choose split index
    double minOvlp = DBL_MAX, minVol = DBL_MAX;
    for(int splitDis : {0, 1}){
        if(splitDis == 0){
            sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
                if(entryType == EntryType::Query)
                    return queries_[idx1]->low_[splitDim] < queries_[idx2]->low_[splitDim];
                else
                    return treeNodes_[idx1]->low_[splitDim] < treeNodes_[idx2]->low_[splitDim];
            });
        }else{
            sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
                if(entryType == EntryType::Query)
                    return queries_[idx1]->top_[splitDim] < queries_[idx2]->top_[splitDim];
                else
                    return treeNodes_[idx1]->top_[splitDim] < treeNodes_[idx2]->top_[splitDim];
            });
        }

        Rectangle recPrefix = mergeRange(node,0, RTreeNode::minEntry_-1);
        Rectangle recSuffix = mergeRange(node,RTreeNode::maxEntry_-RTreeNode::minEntry_+1, node->entryNum_);
        for(int candidate = RTreeNode::minEntry_-1; candidate < RTreeNode::maxEntry_-RTreeNode::minEntry_+1; ++candidate){
            if(entryType == EntryType::Query)
                recPrefix.merge(*queries_[entryIDList[candidate]]);
            else
                recPrefix.merge(*treeNodes_[entryIDList[candidate]]);
            Rectangle recRemaining(recSuffix);
            recRemaining.merge(mergeRange(node,candidate+1, RTreeNode::maxEntry_-RTreeNode::minEntry_+1));
            Rectangle ovlpRect = recPrefix.getOverlap(recRemaining);
//            assert(ovlp >= 0.0);
            if(!ovlpRect.isValid() && recPrefix.getVolume() + recRemaining.getVolume() < minVol){
                minOvlp = 0;
                minVol = recPrefix.getVolume() + recRemaining.getVolume();
                rect1.setBound(recPrefix.low_, recPrefix.top_);
                rect2.setBound(recRemaining.low_, recRemaining.top_);
                group1.assign(entryIDList.begin(), entryIDList.begin() + candidate + 1);
                group2.assign(entryIDList.begin() + candidate + 1, entryIDList.end());
            }else if(ovlpRect.isValid()){
                double ovlp = ovlpRect.getVolume();
                if(ovlp < minOvlp){
                    minOvlp = ovlp;
                    minVol = recPrefix.getVolume() + recRemaining.getVolume();
                    rect1.setBound(recPrefix.low_, recPrefix.top_);
                    rect2.setBound(recRemaining.low_, recRemaining.top_);
                    group1.assign(entryIDList.begin(), entryIDList.begin() + candidate + 1);
                    group2.assign(entryIDList.begin() + candidate + 1, entryIDList.end());
                }else if(ovlp == minOvlp && recPrefix.getVolume() + recRemaining.getVolume() < minVol){
                    minVol = recPrefix.getVolume() + recRemaining.getVolume();
                    rect1.setBound(recPrefix.low_, recPrefix.top_);
                    rect2.setBound(recRemaining.low_, recRemaining.top_);
                    group1.assign(entryIDList.begin(), entryIDList.begin() + candidate + 1);
                    group2.assign(entryIDList.begin() + candidate + 1, entryIDList.end());
                }
            }
        }
    }
}

void RSTree::reinsert(vector<ReInsertItem> &reinsertList){
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
            RTreeNode* subtree = treeNodes_[item.itemID_];
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
        if(isOverflowed){
            if(node->nodeID_ == root_){
                // root split
                splitNode(node);
            }else{
                unordered_set<int> curReinsert;
                forcedReinsert(node, curReinsert);
            }
        }
    }
    RTreeNode *tmp = treeNodes_[root_];
    if(tmp->entryNum_ == 1){
        --height_;
        freeNodeID_.push(root_);
        root_ = tmp->entries_[0];
    }
}