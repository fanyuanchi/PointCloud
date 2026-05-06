#include "R-Tree.h"

SplitType RTree::splitType_ =  SplitType::Greene;

RTreeNode* RTree::chooseSubtree(Rectangle* rect, RTreeNode* node) {
    double minEnlargement = DBL_MAX, difference;
    int chosenID = -1;
    RTreeNode *subtree;
    for(int &nodeID : node->entries_){
        subtree = treeNodes_[nodeID];
        difference = subtree->getMerge(*rect).getVolume() - subtree->getVolume();
        if(difference < minEnlargement){
            minEnlargement = difference;
            chosenID = nodeID;
        }
    }
    return treeNodes_[chosenID];
}

RTreeNode* RTree::splitNode(RTreeNode *node) {
    vector<int> group1, group2;
    Rectangle rect1, rect2;

    if(splitType_ == SplitType::Quadratic){
        partitionQuadratic(node, group1, group2, rect1, rect2);
    }else if(splitType_ == SplitType::Linear){
        partitionLinear(node, group1, group2, rect1, rect2);
    }else if(splitType_ == SplitType::Greene) {
        partitionGreene(node, group1, group2, rect1, rect2);
    }else if(splitType_ == SplitType::MinOverlap){
        partitionMinOverlap(node, group1, group2, rect1, rect2);
    }else{
        throw std::runtime_error("invalid split type");
    }

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

void RTree::partitionQuadratic(RTreeNode* node,
                               vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2){
    if(node->height_ == 0){
        double maxWaste = -DBL_MAX;
        int seed1, seed2;
        Rectangle rectangle1, rectangle2;
        for(int idx1 = 0; idx1 < node->entryNum_; ++idx1){
            for(int idx2 = idx1+1; idx2 < node->entryNum_; ++idx2){
                Rectangle rectangle(*queries_[node->entries_[idx1]]); rectangle.merge(*queries_[node->entries_[idx2]]);
                double waste = rectangle.getVolume()
                               - queries_[node->entries_[idx1]]->getVolume() - queries_[node->entries_[idx2]]->getVolume();
                if(waste > maxWaste){
                    seed1 = node->entries_[idx1]; seed2 = node->entries_[idx2];
                    rect1.setBound(*queries_[seed1]); rect2.setBound(*queries_[seed2]);
                    maxWaste = waste;
                }
            }
        }
        group1.push_back(seed1); group2.push_back(seed2);
        list<int> remaining;
        for(int &entryId : node->entries_) if(entryId != seed1 && entryId != seed2) remaining.push_back(entryId);
        // pick next, choose the one with maximum difference between two merged volume
        while(!remaining.empty()){
            // to make the two groups' sizes no smaller than TreeNode::minEntry
            if(group1.size() + remaining.size() == RTreeNode::minEntry_){
                for(int &idx : remaining){ rect1.merge(*queries_[idx]); group1.push_back(idx);}
                break;
            }
            if(group2.size() + remaining.size() == RTreeNode::minEntry_){
                for(int &idx : remaining){ rect2.merge(*queries_[idx]); group2.push_back(idx);}
                break;
            }
            double d1, d2, v1, v2;
            double difference, maxDifference = -DBL_MAX;
            list<int>::iterator iter;
            int nextEntry;
            for(auto it = remaining.begin(); it != remaining.end(); ++it){
                rectangle1.setBound(*queries_[*it]);
                d1 = rect1.getMerge(rectangle1).getVolume(); d2 = rect2.getMerge(rectangle1).getVolume();
                double difference = abs(d1 - d2);
                if(difference > maxDifference){
                    maxDifference = difference;
                    iter = it;
                    v1 = d1; v2 = d2;
                    rectangle2 = rectangle1;
                    nextEntry = *it;
                }
            }
            remaining.erase(iter);
            if(v1 < v2){ group1.push_back(nextEntry); rect1.merge(rectangle2); }
            else if(v1 > v2){ group2.push_back(nextEntry); rect2.merge(rectangle2);}
            else{
                if(group1.size() < group2.size()){ group1.push_back(nextEntry); rect1.merge(rectangle2);}
                else{ group2.push_back(nextEntry); rect2.merge(rectangle2);}
            }
        }
    }else {
        double maxWaste = -DBL_MAX;
        int seed1, seed2;
        Rectangle rectangle1, rectangle2;
        for(int idx1 = 0; idx1 < node->entryNum_; ++idx1){
            for(int j = idx1+1; j < node->entryNum_; ++j){
                Rectangle rectangle(*treeNodes_[node->entries_[idx1]]);
                rectangle.merge(*treeNodes_[node->entries_[j]]);
                double waste = rectangle.getVolume()
                               - treeNodes_[node->entries_[idx1]]->getVolume() - treeNodes_[node->entries_[j]]->getVolume();
                if(waste > maxWaste ){
                    seed1 = node->entries_[idx1]; seed2 = node->entries_[j];
                    rect1.setBound(*treeNodes_[seed1]);
                    rect2.setBound(*treeNodes_[seed2]);
                    maxWaste = waste;
                }
            }
        }
        group1.push_back(seed1); group2.push_back(seed2);
        list<int> remaining;
        for(int &entryId : node->entries_){
            if(entryId != seed1 && entryId != seed2) remaining.push_back(entryId);
        }
        // pick next, choose the one with maximum difference between two merged volume
        while(!remaining.empty()){
            // to make the two groups' sizes no smaller than TreeNode::minEntry
            if(group1.size() + remaining.size() == RTreeNode::minEntry_){
                for(int &idx : remaining){ rect1.merge(*treeNodes_[idx]); group1.push_back(idx);}
                break;
            }
            if(group2.size() + remaining.size() == RTreeNode::minEntry_){
                for(int &idx : remaining){ rect2.merge(*treeNodes_[idx]); group2.push_back(idx);}
                break;
            }
            double d1, d2, v1, v2;
            double difference, maxDifference = -DBL_MAX;
            list<int>::iterator iter;
            int nextEntry;
            for(auto it = remaining.begin(); it != remaining.end(); ++it){
                rectangle1 = *treeNodes_[*it];
                d1 = rect1.getMerge(rectangle1).getVolume(); d2 = rect2.getMerge(rectangle1).getVolume();
                double difference = abs(d1 - d2);
                if(difference > maxDifference){
                    maxDifference = difference;
                    iter = it;
                    v1 = d1; v2 = d2;
                    rectangle2 = rectangle1;
                    nextEntry = *it;
                }
            }
            remaining.erase(iter);
            if(v1 < v2){ group1.push_back(nextEntry); rect1.merge(rectangle2);}
            else if(v1 > v2){ group2.push_back(nextEntry); rect2.merge(rectangle2);}
            else{
                if(group1.size() < group2.size()){ group1.push_back(nextEntry); rect1.merge(rectangle2);}
                else{ group2.push_back(nextEntry); rect2.merge(rectangle2);}
            }
        }
    }
}

void RTree::partitionLinear(RTreeNode* node,
                            vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2){

}

void RTree::partitionGreene(RTreeNode* node,
                            vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2){
    EntryType entryType = node->height_ == 0 ? EntryType::Query : EntryType::Node;
    int splitDim;
    double maxSpan = -DBL_MAX;
    vector<int>& entryIDList = node->entries_;
    for (int dim = 0; dim < DIM; dim++) {
        double span = node->top_[dim] - node->low_[dim];
        if (span > maxSpan) {
            maxSpan = span;
            splitDim = dim;
        }
    }
    sort(entryIDList.begin(), entryIDList.end(),[&](const int& idx1, const int& idx2)->bool {
        double c1, c2;
        if(entryType == EntryType::Query){
            c1 = (queries_[idx1]->low_[splitDim] + queries_[idx1]->top_[splitDim])*0.5;
            c2 = (queries_[idx2]->low_[splitDim] + queries_[idx2]->top_[splitDim])*0.5;
        }else{
            c1 = (treeNodes_[idx1]->low_[splitDim] + treeNodes_[idx1]->top_[splitDim])*0.5;
            c2 = (treeNodes_[idx2]->low_[splitDim] + treeNodes_[idx2]->top_[splitDim])*0.5;
        }
        if(c1 != c2) return c1 < c2;
        if(entryType == EntryType::Query) return queries_[idx1]->queryID_ < queries_[idx2]->queryID_;
        return treeNodes_[idx1]->nodeID_ < treeNodes_[idx2]->nodeID_;
    });

    double minOverlap = DBL_MAX;
    int splitLoc;

    Rectangle recPrefix = mergeRange(node,0, RTreeNode::minEntry_-1);
    Rectangle recSuffix = mergeRange(node,RTreeNode::maxEntry_ - RTreeNode::minEntry_+1, node->entryNum_);
    for(int candidate = RTreeNode::minEntry_-1; candidate < RTreeNode::maxEntry_ - RTreeNode::minEntry_+1; ++candidate){
        if(entryType == EntryType::Query)
            recPrefix.merge(*queries_[entryIDList[candidate]]);
        else
            recPrefix.merge(*treeNodes_[entryIDList[candidate]]);

        Rectangle recRemaining(recSuffix);
        recRemaining.merge(mergeRange(node,candidate+1, RTreeNode::maxEntry_ - RTreeNode::minEntry_+1));
        double overlap = recRemaining.getOverlap(recPrefix).getVolume();
        if (overlap < minOverlap) {
            minOverlap = overlap;
            rect1.setBound(recPrefix.low_, recPrefix.top_);
            rect2.setBound(recRemaining.low_, recRemaining.top_);
            splitLoc = candidate;
        }
    }
    group1.assign(entryIDList.begin(), entryIDList.begin() + splitLoc+1);
    group2.assign(entryIDList.begin() + splitLoc+1, entryIDList.end());
}

void RTree::partitionMinOverlap(RTreeNode* node,
                                vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2){

}
