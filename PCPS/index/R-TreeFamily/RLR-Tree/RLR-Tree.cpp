#include "RLR-Tree.h"

int RLRTree::actionSpace_ = 2;
string RLRTree::insertModelPath_ =
        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/RiverBank/insertModel.pt";
string RLRTree::splitModelPath_ =
        "/home/data/home/fanyuanchi/CLionProjects/PCPS/index/R-TreeFamily/RLR-Tree/Model/RiverBank/splitModel.pt";

double SplitOverlap(const Rectangle& rect1, const Rectangle& rect2){
    auto overlap = rect1.getOverlap(rect2);
    if(!overlap.isValid()){
        return 0.0;
    }
    return overlap.getVolume();
}

RTreeNode* RLRTree::chooseSubtree(Rectangle* rect, RTreeNode* node) {
    assert(node->height_ > 0);
    torch::InferenceMode guard;
    vector<RTreeNode*> COV;
    int chosenID = -1;
    for(int &nodeID : node->entries_){
        if(treeNodes_[nodeID]->isContain(*rect)){
            COV.push_back(treeNodes_[nodeID]);
        }
    }
    if(!COV.empty()){ // if any child contains #rect
        auto minVolume = DBL_MAX;
        for(auto nodePtr: COV){
            double volume = nodePtr->getVolume();
            if(volume < minVolume){
                minVolume = volume;
                chosenID = nodePtr->nodeID_;
            }
        }
    }else{
        vector<int>& entryIDList = node->entries_;
        sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2) -> bool {
            return treeNodes_[idx1]->getMerge(*rect).getVolume() - treeNodes_[idx1]->getVolume()
                   < treeNodes_[idx2]->getMerge(*rect).getVolume() - treeNodes_[idx2]->getVolume();
        });

        int stateShape = RLRTree::actionSpace_ * 4;
        vector<double> states(stateShape);
        double volumeNormalize = -DBL_MAX, marginNormalize = -DBL_MAX, overlapNormalize = -DBL_MAX, occupancyNormalize = -DBL_MAX;
        for(int idx1 = 0; idx1 < RLRTree::actionSpace_; ++idx1){
            RTreeNode* curNode = treeNodes_[entryIDList[idx1]];
            int loc = idx1 * 4;
            Rectangle newRect(curNode->getMerge(*rect));
            states[loc] = newRect.getVolume() - curNode->getVolume();
            states[loc+1] = newRect.getPerimeter() - curNode->getPerimeter();
            double oldOverlap = 0, newOverlap = 0;
            for(int idx2 = 0; idx2 < node->entryNum_; ++idx2){
                if(idx1 == idx2) continue;
                RTreeNode* otherNode = treeNodes_[entryIDList[idx2]];
                oldOverlap += SplitOverlap(*curNode, *otherNode);
                newOverlap += SplitOverlap(newRect, *otherNode);
            }
            states[loc+2] = newOverlap - oldOverlap;
            states[loc+3] = 1.0 * curNode->entryNum_ / RTreeNode::maxEntry_;

            volumeNormalize = max(volumeNormalize, states[loc]);
            marginNormalize = max(marginNormalize, states[loc+1]);
            overlapNormalize = max(overlapNormalize, states[loc+2]);
            occupancyNormalize = max(occupancyNormalize, states[loc+3]);
        }
        for(int idx = 0; idx < RLRTree::actionSpace_; ++idx){
            states[idx * 4] = states[idx * 4] / (volumeNormalize + 0.001);
            states[idx * 4 + 1] = states[idx * 4 + 1] / (marginNormalize + 0.001);
            states[idx * 4 + 2] = states[idx * 4 + 2] / (overlapNormalize + 0.001);
            states[idx * 4 + 3] = states[idx * 4 + 3] / (occupancyNormalize + 0.001);
        }

        torch::NoGradGuard no_grad;
        auto stateTensor = torch::tensor(states, torch::kFloat32).unsqueeze(0);
        auto qValues = insertModel_->forward(stateTensor);
        int action = qValues.argmax(1).item<int>();
        chosenID = entryIDList[action];
    }

    return treeNodes_[chosenID];
}

void RLRTree::partition(RTreeNode* node, vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2) {
    EntryType entryType = node->height_ == 0 ? EntryType::Query : EntryType::Node;
    vector<SplitLocation> splitLocations((RTreeNode::maxEntry_ - 2 * RTreeNode::minEntry_ + 2) * 2 * DIM);
    vector<int>& entryIDList = node->entries_;

    int loc = 0;
    for(int dim = 0; dim < DIM; ++dim){
        for(int splitDis : {0, 1}) {
            if (splitDis == 0) { // sort by lower bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                    if (entryType == EntryType::Query) {
                        if (queries_[idx1]->low_[dim] != queries_[idx2]->low_[dim]) return queries_[idx1]->low_[dim] < queries_[idx2]->low_[dim];
                        if (queries_[idx1]->top_[dim] != queries_[idx2]->top_[dim]) return queries_[idx1]->top_[dim] < queries_[idx2]->top_[dim];
                        for (int d = 0; d < DIM; ++d) {
                            if (d == dim) continue;
                            if (queries_[idx1]->low_[d] != queries_[idx2]->low_[d]) return queries_[idx1]->low_[d] < queries_[idx2]->low_[d];
                            if (queries_[idx1]->top_[d] != queries_[idx2]->top_[d]) return queries_[idx1]->top_[d] < queries_[idx2]->top_[d];
                        }
                        return queries_[idx1]->queryID_ < queries_[idx2]->queryID_;
                    } else {
                        if (treeNodes_[idx1]->low_[dim] != treeNodes_[idx2]->low_[dim]) return treeNodes_[idx1]->low_[dim] < treeNodes_[idx2]->low_[dim];
                        if (treeNodes_[idx1]->top_[dim] != treeNodes_[idx2]->top_[dim]) return treeNodes_[idx1]->top_[dim] < treeNodes_[idx2]->top_[dim];
                        for (int d = 0; d < DIM; ++d) {
                            if (d == dim) continue;
                            if (treeNodes_[idx1]->low_[d] != treeNodes_[idx2]->low_[d]) return treeNodes_[idx1]->low_[d] < treeNodes_[idx2]->low_[d];
                            if (treeNodes_[idx1]->top_[d] != treeNodes_[idx2]->top_[d]) return treeNodes_[idx1]->top_[d] < treeNodes_[idx2]->top_[d];
                        }
                        return treeNodes_[idx1]->nodeID_ < treeNodes_[idx2]->nodeID_;
                    }
                });
            } else { // sort by upper bound
                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                    if (entryType == EntryType::Query) {
                        if (queries_[idx1]->top_[dim] != queries_[idx2]->top_[dim]) return queries_[idx1]->top_[dim] < queries_[idx2]->top_[dim];
                        if (queries_[idx1]->low_[dim] != queries_[idx2]->low_[dim]) return queries_[idx1]->low_[dim] < queries_[idx2]->low_[dim];
                        for (int d = 0; d < DIM; ++d) {
                            if (d == dim) continue;
                            if (queries_[idx1]->top_[d] != queries_[idx2]->top_[d]) return queries_[idx1]->top_[d] < queries_[idx2]->top_[d];
                            if (queries_[idx1]->low_[d] != queries_[idx2]->low_[d]) return queries_[idx1]->low_[d] < queries_[idx2]->low_[d];
                        }
                        return queries_[idx1]->queryID_ < queries_[idx2]->queryID_;
                    } else {
                        if (treeNodes_[idx1]->top_[dim] != treeNodes_[idx2]->top_[dim]) return treeNodes_[idx1]->top_[dim] < treeNodes_[idx2]->top_[dim];
                        if (treeNodes_[idx1]->low_[dim] != treeNodes_[idx2]->low_[dim]) return treeNodes_[idx1]->low_[dim] < treeNodes_[idx2]->low_[dim];
                        for (int d = 0; d < DIM; ++d) {
                            if (d == dim) continue;
                            if (treeNodes_[idx1]->top_[d] != treeNodes_[idx2]->top_[d]) return treeNodes_[idx1]->top_[d] < treeNodes_[idx2]->top_[d];
                            if (treeNodes_[idx1]->low_[d] != treeNodes_[idx2]->low_[d]) return treeNodes_[idx1]->low_[d] < treeNodes_[idx2]->low_[d];
                        }
                        return treeNodes_[idx1]->nodeID_ < treeNodes_[idx2]->nodeID_;
                    }
                });
            }

            Rectangle recPrefix = mergeRange(node, 0, RTreeNode::minEntry_ - 1);
            Rectangle recSuffix = mergeRange(node, RTreeNode::maxEntry_ - RTreeNode::minEntry_ + 1, node->entryNum_);
            for (int candidate = RTreeNode::minEntry_ - 1; candidate < RTreeNode::maxEntry_ - RTreeNode::minEntry_ + 1; ++candidate) {
                if (entryType == EntryType::Query)
                    recPrefix.merge(*queries_[entryIDList[candidate]]);
                else
                    recPrefix.merge(*treeNodes_[entryIDList[candidate]]);
                Rectangle recRemaining(recSuffix);
                recRemaining.merge(mergeRange(node, candidate + 1, RTreeNode::maxEntry_ - RTreeNode::minEntry_ + 1));
                splitLocations[loc].perimeter1 = max(recPrefix.getPerimeter(), recRemaining.getPerimeter());
                splitLocations[loc].perimeter2 = min(recPrefix.getPerimeter(), recRemaining.getPerimeter());
                splitLocations[loc].area1 = max(recPrefix.getVolume(), recRemaining.getVolume());
                splitLocations[loc].area2 = min(recPrefix.getVolume(), recRemaining.getVolume());
                splitLocations[loc].overlap = SplitOverlap(recPrefix, recRemaining);
                splitLocations[loc].location = candidate;
                splitLocations[loc].dimension = dim * 2 + splitDis;
                ++loc;
            }
        }
    }
    assert(loc == (RTreeNode::maxEntry_ - 2 * RTreeNode::minEntry_ + 2) * 2 * DIM);

    int nonOverlapSplitNum = 0;
    for(auto& splitLocation : splitLocations){
        if(splitLocation.overlap == 0) ++nonOverlapSplitNum;
    }

    int splitLoc = 0;
    int splitDim = 0;
    if(nonOverlapSplitNum > 1){
        torch::InferenceMode guard;
        int stateShape = RLRTree::actionSpace_ * 4;
        vector<double> states(stateShape);
        vector<int> candidateSplitAction(RLRTree::actionSpace_);
        vector<pair<double, int>> zeroOverlapSplits;
        for(int idx = 0; idx < splitLocations.size(); ++idx){
            if(splitLocations[idx].overlap == 0){
                double perimeter = max(splitLocations[idx].perimeter1, splitLocations[idx].perimeter2);
                zeroOverlapSplits.emplace_back(perimeter, idx);
            }
        }
        sort(zeroOverlapSplits.begin(), zeroOverlapSplits.end());
        double maxArea = -DBL_MAX, minArea = DBL_MAX, maxPerimeter = -DBL_MAX, minPerimeter = DBL_MAX;

        for(int idx = 0; idx < RLRTree::actionSpace_; ++idx){
            int splitIDX = zeroOverlapSplits[idx].second;
            candidateSplitAction[idx] = splitIDX;
            states[idx * 4] = splitLocations[splitIDX].area1;
            states[idx * 4 + 1] = splitLocations[splitIDX].area2;
            states[idx * 4 + 2] = splitLocations[splitIDX].perimeter1;
            states[idx * 4 + 3] = splitLocations[splitIDX].perimeter2;
            maxArea = max(maxArea, states[idx * 4]);
            minArea = min(minArea, states[idx * 4 + 1]);
            maxPerimeter = max(maxPerimeter, states[idx * 4 + 2]);
            minPerimeter = min(minPerimeter, states[idx * 4 + 3]);
        }
        for(int idx = 0; idx < RLRTree::actionSpace_; ++idx){
            states[idx * 4] = (states[idx * 4] - minArea) / (maxArea - minArea + 0.001);
            states[idx * 4 + 1] = (states[idx * 4 + 1] - minArea) / (maxArea - minArea + 0.001);
            states[idx * 4 + 2] = (states[idx * 4 + 2] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
            states[idx * 4 + 3] = (states[idx * 4 + 3] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
        }

        torch::NoGradGuard no_grad;
        auto stateTensor = torch::tensor(states, torch::kFloat32).unsqueeze(0);
        auto qValues = splitModel_->forward(stateTensor);
        int action = qValues.argmax(1).item<int>();

        assert(candidateSplitAction[action] >= 0 && candidateSplitAction[action] < splitLocations.size());
        splitLoc = splitLocations[candidateSplitAction[action]].location;
        splitDim = splitLocations[candidateSplitAction[action]].dimension;
    }else {
        auto minOverlap = DBL_MAX;
        for(auto& splitLocation : splitLocations){
            if(splitLocation.overlap < minOverlap){
                minOverlap = splitLocation.overlap;
                splitLoc = splitLocation.location;
                splitDim = splitLocation.dimension;
            }
        }
    }

    int splitDis = splitDim % 2;
    splitDim /= 2;
    if (splitDis == 0) { // sort by lower bound
        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
            if (entryType == EntryType::Query) {
                if (queries_[idx1]->low_[splitDim] != queries_[idx2]->low_[splitDim]) return queries_[idx1]->low_[splitDim] < queries_[idx2]->low_[splitDim];
                if (queries_[idx1]->top_[splitDim] != queries_[idx2]->top_[splitDim]) return queries_[idx1]->top_[splitDim] < queries_[idx2]->top_[splitDim];
                for (int d = 0; d < DIM; ++d) {
                    if (d == splitDim) continue;
                    if (queries_[idx1]->low_[d] != queries_[idx2]->low_[d]) return queries_[idx1]->low_[d] < queries_[idx2]->low_[d];
                    if (queries_[idx1]->top_[d] != queries_[idx2]->top_[d]) return queries_[idx1]->top_[d] < queries_[idx2]->top_[d];
                }
                return queries_[idx1]->queryID_ < queries_[idx2]->queryID_;
            } else {
                if (treeNodes_[idx1]->low_[splitDim] != treeNodes_[idx2]->low_[splitDim]) return treeNodes_[idx1]->low_[splitDim] < treeNodes_[idx2]->low_[splitDim];
                if (treeNodes_[idx1]->top_[splitDim] != treeNodes_[idx2]->top_[splitDim]) return treeNodes_[idx1]->top_[splitDim] < treeNodes_[idx2]->top_[splitDim];
                for (int d = 0; d < DIM; ++d) {
                    if (d == splitDim) continue;
                    if (treeNodes_[idx1]->low_[d] != treeNodes_[idx2]->low_[d]) return treeNodes_[idx1]->low_[d] < treeNodes_[idx2]->low_[d];
                    if (treeNodes_[idx1]->top_[d] != treeNodes_[idx2]->top_[d]) return treeNodes_[idx1]->top_[d] < treeNodes_[idx2]->top_[d];
                }
                return treeNodes_[idx1]->nodeID_ < treeNodes_[idx2]->nodeID_;
            }
        });
    } else { // sort by upper bound
        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
            if (entryType == EntryType::Query) {
                if (queries_[idx1]->top_[splitDim] != queries_[idx2]->top_[splitDim]) return queries_[idx1]->top_[splitDim] < queries_[idx2]->top_[splitDim];
                if (queries_[idx1]->low_[splitDim] != queries_[idx2]->low_[splitDim]) return queries_[idx1]->low_[splitDim] < queries_[idx2]->low_[splitDim];
                for (int d = 0; d < DIM; ++d) {
                    if (d == splitDim) continue;
                    if (queries_[idx1]->top_[d] != queries_[idx2]->top_[d]) return queries_[idx1]->top_[d] < queries_[idx2]->top_[d];
                    if (queries_[idx1]->low_[d] != queries_[idx2]->low_[d]) return queries_[idx1]->low_[d] < queries_[idx2]->low_[d];
                }
                return queries_[idx1]->queryID_ < queries_[idx2]->queryID_;
            } else {
                if (treeNodes_[idx1]->top_[splitDim] != treeNodes_[idx2]->top_[splitDim]) return treeNodes_[idx1]->top_[splitDim] < treeNodes_[idx2]->top_[splitDim];
                if (treeNodes_[idx1]->low_[splitDim] != treeNodes_[idx2]->low_[splitDim]) return treeNodes_[idx1]->low_[splitDim] < treeNodes_[idx2]->low_[splitDim];
                for (int d = 0; d < DIM; ++d) {
                    if (d == splitDim) continue;
                    if (treeNodes_[idx1]->top_[d] != treeNodes_[idx2]->top_[d]) return treeNodes_[idx1]->top_[d] < treeNodes_[idx2]->top_[d];
                    if (treeNodes_[idx1]->low_[d] != treeNodes_[idx2]->low_[d]) return treeNodes_[idx1]->low_[d] < treeNodes_[idx2]->low_[d];
                }
                return treeNodes_[idx1]->nodeID_ < treeNodes_[idx2]->nodeID_;
            }
        });
    }

    group1.assign(entryIDList.begin(), entryIDList.begin() + splitLoc + 1);
    group2.assign(entryIDList.begin() + splitLoc + 1, entryIDList.end());
    rect1.setBound(mergeRange(node, 0, splitLoc + 1));
    rect2.setBound(mergeRange(node, splitLoc + 1, node->entryNum_));
    assert(group1.size() + group2.size() == entryIDList.size());
}
