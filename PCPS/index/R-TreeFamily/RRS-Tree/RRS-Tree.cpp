#include "RRS-Tree.h"

double RRSTree::RRs_ = 0.5;
double RRSTree::y1_ = exp(-1.0 / RRSTree::RRs_ / RRSTree::RRs_);
double RRSTree::ys_ = 1.0 / (1.0 - RRSTree::y1_);

double RRSTree::getDeltaOverlappedVolume(RTreeNode* node, int targetIDX, int otherIDX, Rectangle* rect) const{
//    assert(node->height_ > 0);
    vector<int>& entryIDList = node->entries_;
    Rectangle rect1 = treeNodes_[entryIDList[targetIDX]]->getMerge(*rect).getOverlap(*treeNodes_[entryIDList[otherIDX]]);
    Rectangle rect2 = treeNodes_[entryIDList[targetIDX]]->getOverlap(*treeNodes_[entryIDList[otherIDX]]);
    double volume1 = rect1.isValid() ? rect1.getVolume() : 0;
    double volume2 = rect2.isValid() ? rect2.getVolume() : 0;
    return volume1 - volume2;
}

double RRSTree::getDeltaOverlappedPerimeter(RTreeNode* node, int targetIDX, int otherIDX, Rectangle* rect) const{
//    assert(node->height_ > 0);
    vector<int>& entryIDList = node->entries_;
    Rectangle rect1 = treeNodes_[entryIDList[targetIDX]]->getMerge(*rect).getOverlap(*treeNodes_[entryIDList[otherIDX]]);
    Rectangle rect2 = treeNodes_[entryIDList[targetIDX]]->getOverlap(*treeNodes_[entryIDList[otherIDX]]);
    double perimeter1 = rect1.isValid() ? rect1.getPerimeter() : 0;
    double perimeter2 = rect2.isValid() ? rect2.getPerimeter() : 0;
    return perimeter1 - perimeter2;
}

double RRSTree::getDeltaOverlappedPerimeter(RTreeNode* node, int targetIDX, int startIDX, int endIDX, Rectangle* rect) const{
//    assert(node->height_ > 0);
    double sum = 0.0;
    for(int idx = startIDX; idx < endIDX; ++idx){
        if(idx == targetIDX) continue;
        sum += getDeltaOverlappedPerimeter(node, targetIDX, idx, rect);
    }
    return sum;
}

void RRSTree::CheckComp(unordered_set<int>& CAND, vector<double>& ovlp, RTreeNode* node,
                        int targetIDX, Rectangle* rect, bool usePerimeter, int priority, bool &success, int& candidate) const{
    CAND.insert(targetIDX);
    double ovlpf;
    for(int entryIDX = 0; entryIDX < priority; ++entryIDX){
        if(usePerimeter){ // if any treeNode's volume is zero, consider the perimeter
            ovlpf = getDeltaOverlappedPerimeter(node, targetIDX, entryIDX, rect);
        }else{ // if no treeNode's volume is zero, just consider the volume
            ovlpf = getDeltaOverlappedVolume(node, targetIDX, entryIDX, rect);
        }
        ovlp[targetIDX] += ovlpf;
        if(ovlpf != 0 && !CAND.contains(entryIDX)){
            CheckComp(CAND, ovlp, node, entryIDX, rect, usePerimeter, priority, success, candidate);
            if(success){
                break;
            }
        }
    }
    if(ovlp[targetIDX] == 0){
        candidate = targetIDX;
        success = true;
    }
}

RTreeNode* RRSTree::chooseSubtree(Rectangle* rect, RTreeNode* node) {
    vector<RTreeNode*> COV;
    int chosenID = -1;
    for(int &nodeID : node->entries_){
        if(treeNodes_[nodeID]->isContain(*rect)){
            COV.push_back(treeNodes_[nodeID]);
        }
    }
    if(!COV.empty()){ // if any child contains #rect
        double minVolume = DBL_MAX, minPerimeter = DBL_MAX;
        int tmpID1 = -1, tmpID2 = -1;
        for(auto nodePtr: COV){
            double volume = nodePtr->getVolume();
            double perimeter = nodePtr->getPerimeter();
            if(volume < minVolume){
                minVolume = volume;
                tmpID1 = nodePtr->nodeID_;
            }
            if(perimeter < minPerimeter){
                minPerimeter = perimeter;
                tmpID2 = nodePtr->nodeID_;
            }
        }
        chosenID = minVolume > 0 ? tmpID1 : tmpID2;
    }else{
        vector<int>& entryIDList = node->entries_;
        sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
            double deltaP1 = treeNodes_[idx1]->getMerge(*rect).getPerimeter() - treeNodes_[idx1]->getPerimeter();
            double deltaP2 = treeNodes_[idx2]->getMerge(*rect).getPerimeter() - treeNodes_[idx2]->getPerimeter();
            return deltaP1 < deltaP2;
        });

        if(getDeltaOverlappedPerimeter(node, 0, 0, node->entryNum_, rect) == 0){
            chosenID = entryIDList[0];
        }else{
            int priority = node->entryNum_;
            for(int idx = 1; idx < node->entryNum_; ++idx){
                if(getDeltaOverlappedPerimeter(node, 0, idx, rect) != 0){
                    priority = idx;
                }
            }
            if(priority < node->entryNum_) ++priority;
            bool success = false, usePerimeter = false;
            unordered_set<int> CAND;
            int candidate;
            vector<double> ovlp(priority, 0.0);
            for(int idx = 0; idx < priority; ++idx){
                if(treeNodes_[entryIDList[idx]]->getMerge(*rect).getVolume() == 0){
                    usePerimeter = true;
                    break;
                }
            }
            CheckComp(CAND, ovlp, node, 0, rect, usePerimeter, priority, success, candidate);
            if(success){
                chosenID = entryIDList[candidate];
            }else{
                double minOvlp = DBL_MAX;
                int minIDX = INT_MAX;
                for(const int& entryIDX : CAND){
                    if(ovlp[entryIDX] < minOvlp){
                        minIDX = entryIDX;
                        minOvlp = ovlp[entryIDX];
                    }else if(ovlp[entryIDX] == minOvlp){
                        minIDX = entryIDX;
                    }
                }
                chosenID = entryIDList[minIDX];
            }
        }
    }
    return treeNodes_[chosenID];
}

double RRSwf(int candidate, double y1, double ys, double miu, double delta) {
    double xi = static_cast<double>(2.0*candidate) / static_cast<double>(RTreeNode::maxEntry_+1) - 1;
    double wf = ys * (exp(0 - (xi - miu) * (xi - miu) / delta / delta) - y1);
    return wf;
}

void RRSTree::partition(RTreeNode *node,
                        vector<int> &group1, vector<int> &group2, Rectangle &rect1, Rectangle &rect2) {
    EntryType entryType = node->height_ == 0 ? EntryType::Query : EntryType::Node;
    vector<int>& entryIDList = node->entries_;
    int splitDim, splitDis, splitLoc;
    double sumPerimeter, minPerimeter = DBL_MAX;
    for(int dim = 0; dim < DIM; ++dim) {
        for (int dis: {0, 1}) {
            if(dis == 0){ // sort by lower bound
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
            Rectangle recPrefix = mergeRange(node,0,RTreeNode::minEntry_-1);
            Rectangle recSuffix = mergeRange(node,RTreeNode::maxEntry_-RTreeNode::minEntry_+1,node->entryNum_);
            sumPerimeter = 0.0;
            for (int candidate = RTreeNode::minEntry_-1; candidate < RTreeNode::maxEntry_-RTreeNode::minEntry_+1; ++candidate) {
                if(entryType == EntryType::Query)
                    recPrefix.merge(*queries_[entryIDList[candidate]]);
                else
                    recPrefix.merge(*treeNodes_[entryIDList[candidate]]);
                Rectangle recRemaining(recSuffix);
                recRemaining.merge(mergeRange(node,candidate+1,RTreeNode::maxEntry_-RTreeNode::minEntry_+1));
                sumPerimeter += recPrefix.getPerimeter() + recRemaining.getPerimeter();
            }
            if (sumPerimeter < minPerimeter) {
                minPerimeter = sumPerimeter;
                splitDim = dim;
                splitDis = dis;
            }
        }
    }

    double newCenter[DIM], length[DIM];
    double maxPerimeter = 0.0, minSideLength = DBL_MAX;
    for(int dim = 0; dim < DIM; ++dim){
        newCenter[dim] = (node->low_[dim] + node->top_[dim])*0.5;
        length[dim] = node->top_[dim] - node->low_[dim];
        maxPerimeter += 2.0 * length[dim];
        minSideLength = min(minSideLength, length[dim]);
    }
    maxPerimeter -= minSideLength;

    double minWeight = DBL_MAX;

    double asym = 2*(newCenter[splitDim] - node->center_[splitDim]) / length[splitDim];
    double miu = (1-static_cast<double>(2.0*RTreeNode::minEntry_) / static_cast<double>(RTreeNode::maxEntry_+1)) * asym;
    double delta = RRSTree::RRs_ * (1+abs(miu));

    if(splitDis == 0){ // sort by lower bound
        sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
            if(entryType == EntryType::Query)
                return queries_[idx1]->low_[splitDim] < queries_[idx2]->low_[splitDim];
            else
                return treeNodes_[idx1]->low_[splitDim] < treeNodes_[idx2]->low_[splitDim];
        });
    }else{ // sort by upper bound
        sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2)->bool{
            if(entryType == EntryType::Query)
                return queries_[idx1]->top_[splitDim] < queries_[idx2]->top_[splitDim];
            else
                return treeNodes_[idx1]->top_[splitDim] < treeNodes_[idx2]->top_[splitDim];
        });
    }

    Rectangle recPrefix = mergeRange(node,0, RTreeNode::minEntry_-1);
    Rectangle recSuffix = mergeRange(node,RTreeNode::maxEntry_-RTreeNode::minEntry_+1, node->entryNum_);
    for (int candidate = RTreeNode::minEntry_-1; candidate < RTreeNode::maxEntry_-RTreeNode::minEntry_+1; ++candidate) {
        if(entryType == EntryType::Query)
            recPrefix.merge(*queries_[entryIDList[candidate]]);
        else
            recPrefix.merge(*treeNodes_[entryIDList[candidate]]);
        Rectangle recRemaining(recSuffix);
        recRemaining.merge(mergeRange(node,candidate+1,RTreeNode::maxEntry_-RTreeNode::minEntry_+1));
        double w, wg, wf = RRSwf(candidate, y1_, ys_, miu, delta);
        if (recPrefix.isOverlap(recRemaining)) {
            wg = recPrefix.getOverlap(recRemaining).getVolume();
            w = wg / wf;
        }else {
            wg = recPrefix.getPerimeter() + recRemaining.getPerimeter() - maxPerimeter;
            w = wg * wf;
        }
        if (w < minWeight) {
            minWeight = w;
            splitLoc = candidate;
            rect1.setBound(recPrefix);
            rect2.setBound(recRemaining);
        }
    }
    group1.assign(entryIDList.begin(), entryIDList.begin() + splitLoc + 1);
    group2.assign(entryIDList.begin() + splitLoc + 1, entryIDList.end());
}