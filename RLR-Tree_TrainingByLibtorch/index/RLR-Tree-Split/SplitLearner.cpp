#include "SplitLearner.h"

double SplitLearner::ALPHA_ = 0.01;
double SplitLearner::GAMMA_ = 0.8;
double SplitLearner::EPSILON_ = 0.9;
double SplitLearner::EPS_END_ = 0.1;
double SplitLearner::teacherForcing_ = 0.1;
int SplitLearner::partNum_ = 3;
int SplitLearner::actionSpace_ = 2;
int SplitLearner::replaceCnt_ = 30;
int SplitLearner::epoch_ = 2;
int SplitLearner::rewardComFreq_ = 5;

int SplitLearner::chooseAction(const torch::Tensor& insertState){
    torch::NoGradGuard no_grad;
    stepCnt_++;
    double rand = uniform01_(rng_);
    if(rand < EPSILON_) {
        return actionDist_(rng_);
    }
    auto actions = QEval_->forward(insertState.unsqueeze(0));
    return actions.argmax(1).item<int>();
}

void SplitLearner::optimize(){
    if(memory_.curSize() < ReplayMemory::batchSize_) return;
    if(++learnStepCnt_ % replaceCnt_ == 0) {
        torch::NoGradGuard no_grad;
        auto paramsEval = QEval_->named_parameters(true);
        auto paramsNext = QNext_->named_parameters(true);
        for (auto& val : paramsEval) {
            auto& name = val.key();
            auto& tensorEval = val.value();
            auto* tensorNext = paramsNext.find(name);
            TORCH_CHECK(tensorNext != nullptr, "Parameter not found: ", name)
            tensorNext->copy_(tensorEval);
        }
    }
    auto batch = memory_.sampleTransition(rng_);
    std::vector<torch::Tensor> states;
    std::vector<torch::Tensor> nextStates;
    std::vector<int> actions;
    std::vector<float> rewards;
    std::vector<float> dones;

    for(auto& transition : batch) {
        states.push_back(transition.state_);
        nextStates.push_back(transition.nextState_);
        actions.push_back(transition.action_);
        rewards.push_back(transition.reward_);
        dones.push_back(transition.isDone_ ? 1.0f : 0.0f);
    }

    auto stateBatch = torch::stack(states);
    auto nextBatch  = torch::stack(nextStates);

    auto Qpred = QEval_->forward(stateBatch);
    auto actionTensor = torch::tensor(actions).to(torch::kInt64).unsqueeze(1);
    auto QpredChosen = Qpred.gather(1, actionTensor);

    auto Qnext = QNext_->forward(nextBatch).detach();
    auto maxNext = std::get<0>(Qnext.max(1)).unsqueeze(1);

    auto rewardTensor = torch::tensor(rewards).unsqueeze(1);
    auto doneTensor   = torch::tensor(dones).unsqueeze(1);
    auto Qtarget = rewardTensor + GAMMA_ * maxNext * (1 - doneTensor);

    optimizer_->zero_grad();
    auto loss = loss_(QpredChosen, Qtarget);
    loss.backward();
    optimizer_->step();

    if(stepCnt_ % 500 == 0) {
        if(EPSILON_ *0.99> EPS_END_) EPSILON_ *= 0.99;
        else EPSILON_ = EPS_END_;
    }
}

float SplitLearner::computeDenseRewardP2R(vector<Rectangle*>& rectForReward){
    double avgAccessRate = 0;
    for(auto& rect: rectForReward){
        for(int idx = 0; idx < 3; ++idx){
            Point point;
            for(int dim = 0; dim < DIM; ++dim){
                double rand = uniform01_(rng_);
                point.cord_[dim] = rect->low_[dim] + rand * (rect->top_[dim] - rect->low_[dim]);
            }
            double referenceRate = referenceTree_->accessRateStabbing(point);
            double treeRate = tree_->accessRateStabbing(point);
            avgAccessRate += referenceRate - treeRate;
        }
    }
    return static_cast<float>(avgAccessRate / 3.0 / rectForReward.size());
}

float SplitLearner::computeDenseRewardR2R(vector<Rectangle*>& rectForReward){
    double avgAccessRate = 0;
    for(auto& rect: rectForReward){
        Point center = rect->getCenter();
        for(int idx = 0; idx < 3; ++idx){
            Rectangle queryRange;
            DataLoader::generateOneSampledRect(center, queryRange, 4e-4, 6e-4, rng_);
            double referenceRate = referenceTree_->accessRateRange(queryRange);
            double treeRate = tree_->accessRateRange(queryRange);
            avgAccessRate += referenceRate - treeRate;
        }
    }
    return static_cast<float>(avgAccessRate / 3.0 / rectForReward.size());
}

//void SplitLearner::trainP2R(const string& modelPath){
//    double startTime = thread_real_time();
//    int bestEpoch = -1, maxNonNegativeCnt = -1;
//    size_t trainingSetSize = trainingSet_.size();
//    vector<pair<torch::Tensor, int>> stateActionPair;
//    auto *cacheTree = new RTree();
//    DQN QFinal = DQN(SplitLearner::actionSpace_ * 4, 64, SplitLearner::actionSpace_);
//    for(int epoIDX = 0; epoIDX < SplitLearner::epoch_; ++epoIDX){
//        stepCnt_ = 0;
//        int totalCnt = 0, positiveCnt = 0, nonNegativeCnt = 0;
//        for(int partIDX = 0; partIDX < SplitLearner::partNum_; ++partIDX){
//            double ratioForTreeBuild = 1.0 * (partIDX + 1) / (SplitLearner::partNum_);
//            auto buildNum = static_cast<size_t>(ratioForTreeBuild * trainingSetSize);
//
//            tree_->clearTree();
//            referenceTree_->clearTree();
//            cacheTree->clearTree();
//            // Part 1: build the tree
//            for(size_t rectIDX = 0; rectIDX < buildNum; ++rectIDX){
//                tree_->insertRect(*trainingSet_[rectIDX]);
//            }
//
//            // Part 2: identify rectangles that will trigger splitting to the tree from Part 1
//            vector<Rectangle*> rectForTrain;
//            size_t rectForTrainNum = 0;
//            for(size_t rectIDX = buildNum; rectIDX < trainingSetSize; ++rectIDX){
//                Rectangle* r = trainingSet_[rectIDX];
//                // traverse a path to a specific leaf node using function chooseSubtree but do not update the MBR
//                TreeNode *node = tree_->treeNodes_[tree_->root_];
//                // choose a child node of the internal node to insert and update the bound
//                while(node->height_ > 0)
//                    node = tree_->chooseSubtree(r, node);
//                bool isOverflowed = node->entryNum_ + 1 > TreeNode::maxEntry_;
//                if(isOverflowed){
//                    rectForTrain.push_back(r);
//                    ++rectForTrainNum;
//                }else{ // if r do not trigger splitting to the tree, update the MBR back to the root along the insertion path
//                    auto rect = tree_->createRect(r->low_, r->top_);
//                    node->addEntry(rect->rectID_);
//                    assert(node->entryNum_ <= TreeNode::maxEntry_);
//                    node->merge(*rect);
//                    while (node->father_ >= 0){
//                        node = tree_->treeNodes_[node->father_];
//                        node->merge(*rect);
//                    }
//                }
//            }
//            cacheTree->copyTree(tree_);
//            referenceTree_->copyTree(tree_);
//
//            // Part 3: Training
//            int period = 0;
//            vector<Rectangle*> rectForReward;
//            for(size_t trainIDX = 0; trainIDX < rectForTrainNum; ++trainIDX){
//                referenceTree_->insertRect(*rectForTrain[trainIDX]);
//
//                Rectangle* r = rectForTrain[trainIDX];
//                auto rect = tree_->createRect(r->low_, r->top_);
//                // traverse a path to a specific leaf node using function chooseSubtree
//                TreeNode *node = tree_->treeNodes_[tree_->root_];
//                node->merge(*rect);
//                // choose a child node of the internal node to insert and update the bound
//                while(node->height_ > 0){
//                    node = tree_->chooseSubtree(rect, node);
//                    node->merge(*rect);
//                }
//                bool isOverflowed = node->addEntry(rect->rectID_);
//                bool isTrigger = isOverflowed;
//                bool isInLoop = false;
//                while(isOverflowed){
//                    vector<SplitLocation> splitLocations((TreeNode::maxEntry_ - 2 * TreeNode::minEntry_ + 2) * 2 * DIM);
//                    vector<int>& entryIDList = node->entries_;
//
//                    int loc = 0;
//                    for(int dim = 0; dim < DIM; ++dim){
//                        for(int splitDis : {0, 1}) {
//                            if (splitDis == 0) { // sort by lower bound
//                                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
//                                    if (node->height_ == 0) return tree_->objects_[idx1]->low_[dim] < tree_->objects_[idx2]->low_[dim];
//                                    else return tree_->treeNodes_[idx1]->low_[dim] < tree_->treeNodes_[idx2]->low_[dim];
//                                });
//                            } else { // sort by upper bound
//                                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
//                                    if (node->height_ == 0) return tree_->objects_[idx1]->top_[dim] < tree_->objects_[idx2]->top_[dim];
//                                    else return tree_->treeNodes_[idx1]->top_[dim] < tree_->treeNodes_[idx2]->top_[dim];
//                                });
//                            }
//
//                            Rectangle recPrefix = tree_->mergeRange(node, 0, TreeNode::minEntry_ - 1);
//                            Rectangle recSuffix = tree_->mergeRange(node, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1, node->entryNum_);
//                            for (int candidate = TreeNode::minEntry_ - 1; candidate < TreeNode::maxEntry_ - TreeNode::minEntry_ + 1; ++candidate) {
//                                if (node->height_ == 0)
//                                    recPrefix.merge(*(tree_->objects_[entryIDList[candidate]]));
//                                else
//                                    recPrefix.merge(*(tree_->treeNodes_[entryIDList[candidate]]));
//                                Rectangle recRemaining(recSuffix);
//                                recRemaining.merge(tree_->mergeRange(node, candidate + 1, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1));
//                                splitLocations[loc].perimeter1_ = max(recPrefix.getPerimeter(), recRemaining.getPerimeter());
//                                splitLocations[loc].perimeter2_ = min(recPrefix.getPerimeter(), recRemaining.getPerimeter());
//                                splitLocations[loc].volume1_ = max(recPrefix.getVolume(), recRemaining.getVolume());
//                                splitLocations[loc].volume2_ = min(recPrefix.getVolume(), recRemaining.getVolume());
//                                splitLocations[loc].overlap_ = splitByOverlap(recPrefix, recRemaining);
//                                splitLocations[loc].location_ = candidate;
//                                splitLocations[loc].dimension_ = dim * 2 + splitDis;
//                                ++loc;
//                            }
//                        }
//                    }
//                    assert(loc == (TreeNode::maxEntry_ - 2 * TreeNode::minEntry_ + 2) * 2 * DIM);
//
//                    int nonOverlapSplitNum = 0;
//                    for(auto& splitLocation : splitLocations){
//                        if(splitLocation.overlap_ == 0) ++nonOverlapSplitNum;
//                    }
//
//                    int splitLoc = 0;
//                    int splitDim = 0;
//                    if(nonOverlapSplitNum > 1){
//                        isInLoop = true;
//                        torch::NoGradGuard no_grad;
//                        int stateShape = SplitLearner::actionSpace_ * 4;
//                        vector<double> states(stateShape);
//                        vector<int> candidateSplitAction(SplitLearner::actionSpace_);
//                        vector<pair<double, int>> zeroOverlapSplits;
//                        for(int idx = 0; idx < splitLocations.size(); ++idx){
//                            if(splitLocations[idx].overlap_ == 0){
//                                double perimeter = max(splitLocations[idx].perimeter1_, splitLocations[idx].perimeter2_);
//                                zeroOverlapSplits.emplace_back(perimeter, idx);
//                            }
//                        }
//                        sort(zeroOverlapSplits.begin(), zeroOverlapSplits.end());
//                        auto maxArea = -DBL_MAX, minArea = DBL_MAX, maxPerimeter = -DBL_MAX, minPerimeter = DBL_MAX;
//
//                        for(int idx = 0; idx < SplitLearner::actionSpace_; ++idx){
//                            int splitIDX = zeroOverlapSplits[idx].second;
//                            candidateSplitAction[idx] = splitIDX;
//                            states[idx * 4] = splitLocations[splitIDX].volume1_;
//                            states[idx * 4 + 1] = splitLocations[splitIDX].volume2_;
//                            states[idx * 4 + 2] = splitLocations[splitIDX].perimeter1_;
//                            states[idx * 4 + 3] = splitLocations[splitIDX].perimeter2_;
//                            maxArea = max(maxArea, states[idx * 4]);
//                            minArea = min(minArea, states[idx * 4 + 1]);
//                            maxPerimeter = max(maxPerimeter, states[idx * 4 + 2]);
//                            minPerimeter = min(minPerimeter, states[idx * 4 + 3]);
//                        }
//                        for(int idx = 0; idx < SplitLearner::actionSpace_; ++idx){
//                            states[idx * 4] = (states[idx * 4] - minArea) / (maxArea - minArea + 0.001);
//                            states[idx * 4 + 1] = (states[idx * 4 + 1] - minArea) / (maxArea - minArea + 0.001);
//                            states[idx * 4 + 2] = (states[idx * 4 + 2] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
//                            states[idx * 4 + 3] = (states[idx * 4 + 3] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
//                        }
//
//                        auto stateTensor = torch::tensor(states, torch::kFloat32);
//                        int action = -1;
//                        if(trainIDX < SplitLearner::teacherForcing_ * rectForTrainNum){
//                            auto minActionPerimeter = DBL_MAX;
//                            for(int actionIDX = 0; actionIDX < SplitLearner::actionSpace_; ++actionIDX){
//                                if(states[actionIDX * 4 + 2] + states[actionIDX * 4 + 3] < minActionPerimeter){
//                                    minActionPerimeter = states[actionIDX * 4 + 2] + states[actionIDX * 4 + 3];
//                                    action = actionIDX;
//                                }
//                            }
//                        }else{
//                            action = chooseAction(stateTensor);
//                        }
//                        assert(action >= 0 && action < SplitLearner::actionSpace_);
//                        assert(candidateSplitAction[action] >= 0 && candidateSplitAction[action] < splitLocations.size());
//                        stateActionPair.emplace_back(stateTensor, action);
//                        splitLoc = splitLocations[candidateSplitAction[action]].location_;
//                        splitDim = splitLocations[candidateSplitAction[action]].dimension_;
//                    }else {
//                        auto minOverlap = DBL_MAX;
//                        for(auto& splitLocation : splitLocations){
//                            if(splitLocation.overlap_ < minOverlap){
//                                minOverlap = splitLocation.overlap_;
//                                splitLoc = splitLocation.location_;
//                                splitDim = splitLocation.dimension_;
//                            }
//                        }
//                        if(isInLoop){
//                            isInLoop = false;
//                            torch::Tensor finalTensor = torch::zeros({actionSpace_ * 4}, torch::kFloat32);
//                            stateActionPair.emplace_back(finalTensor, -1);
//                        }
//                    }
//
//                    int splitDis = splitDim % 2;
//                    splitDim /= 2;
//                    if (splitDis == 0) { // sort by lower bound
//                        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
//                            if (node->height_ == 0)
//                                return tree_->objects_[idx1]->low_[splitDim] < tree_->objects_[idx2]->low_[splitDim];
//                            else
//                                return tree_->treeNodes_[idx1]->low_[splitDim] < tree_->treeNodes_[idx2]->low_[splitDim];
//                        });
//                    } else { // sort by upper bound
//                        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
//                            if (node->height_ == 0)
//                                return tree_->objects_[idx1]->top_[splitDim] < tree_->objects_[idx2]->top_[splitDim];
//                            else
//                                return tree_->treeNodes_[idx1]->top_[splitDim] < tree_->treeNodes_[idx2]->top_[splitDim];
//                        });
//                    }
//
//                    vector<int> group1(entryIDList.begin(), entryIDList.begin() + splitLoc + 1);
//                    vector<int> group2(entryIDList.begin() + splitLoc + 1, entryIDList.end());
//                    Rectangle rect1(tree_->mergeRange(node, 0, splitLoc + 1));
//                    Rectangle rect2(tree_->mergeRange(node, splitLoc + 1, node->entryNum_));
//
//
//                    TreeNode *sibling = tree_->createNode();
//                    sibling->height_ = node->height_;
//
//                    node->copyEntries(group1);
//                    node->setBound(rect1);
//                    sibling->copyEntries(group2);
//                    sibling->setBound(rect2);
//
//                    if(sibling->height_ > 0){
//                        for(int &entryID : sibling->entries_){
//                            tree_->treeNodes_[entryID]->father_ = sibling->rectID_;
//                        }
//                        for(int &entryID : node->entries_){
//                            tree_->treeNodes_[entryID]->father_ = node->rectID_;
//                        }
//                    }
//                    if(node->father_ >= 0){
//                        tree_->treeNodes_[node->father_]->addEntry(sibling->rectID_);
//                        tree_->treeNodes_[node->father_]->merge(rect2);
//                        sibling->father_ = node->father_;
//                    } else {
//                        TreeNode *newRoot = tree_->createNode();
//                        newRoot->addEntry(node->rectID_);
//                        newRoot->addEntry(sibling->rectID_);
//                        newRoot->setBound(rect1);
//                        newRoot->merge(rect2);
//                        newRoot->height_ = tree_->height_++;
//
//                        tree_->root_ = newRoot->rectID_;
//                        node->father_ = newRoot->rectID_;
//                        sibling->father_ = newRoot->rectID_;
//                    }
//                    node = tree_->treeNodes_[node->father_];
//                    isOverflowed = node->entries_.size() > TreeNode::maxEntry_;
//                }
//                if(isTrigger && !stateActionPair.empty() && stateActionPair.back().second >= 0){
//                    torch::Tensor finalTensor = torch::zeros({actionSpace_ * 4}, torch::kFloat32);
//                    stateActionPair.emplace_back(finalTensor, -1);
//                }
//
//                if(!tree_->checkConsistency()){
//                    cout << "epoIDX: " << epoIDX << ", partIDX: " << partIDX << ", trainIDX: " << trainIDX << endl;
//                    throw std::runtime_error("tree is not well shaped.");
//                }
//                if(!referenceTree_->checkConsistency()){
//                    cout << "epoIDX: " << epoIDX << ", partIDX: " << partIDX << ", trainIDX: " << trainIDX << endl;
//                    throw std::runtime_error("referenceTree is not well shaped.");
//                }
//
//                rectForReward.push_back(r);
//                if(++period % SplitLearner::rewardComFreq_ == 0){
//                    float reward = computeDenseRewardP2R(rectForReward);
//                    for(size_t ind = 0; ind < stateActionPair.size()-1; ++ind){
//                        if (stateActionPair[ind].second == -1) continue;
//                        memory_.storeTransition(stateActionPair[ind].first, stateActionPair[ind].second,
//                                                reward, stateActionPair[ind+1].first);
//                    }
//                    optimize();
//                    referenceTree_->copyTree(cacheTree);
//                    tree_->copyTree(cacheTree);
//                    rectForReward.clear();
//                    stateActionPair.clear();
//
//                    if (reward > 0) positiveCnt++;
//                    if (reward >= 0) nonNegativeCnt++;
//                    totalCnt++;
//                }
//            }
//            cout << "Epoch number: " << epoIDX << ", part index: " << partIDX << endl;
//            cout << "Positive reward number: " << positiveCnt << " / " << totalCnt << endl;
//            cout << "Non negative reward number: " << nonNegativeCnt << " / " << totalCnt << endl << endl;
//        }
//        if(nonNegativeCnt > maxNonNegativeCnt){
//            maxNonNegativeCnt = nonNegativeCnt;
//            bestEpoch = epoIDX;
//            torch::NoGradGuard no_grad;
//            auto paramsEval = QEval_->named_parameters(true);
//            auto paramsFinal = QFinal->named_parameters(true);
//            for (auto& val : paramsEval) {
//                auto& name = val.key();
//                auto& tensorEval = val.value();
//                auto* tensorFinal = paramsFinal.find(name);
//                if (tensorFinal != nullptr) {
//                    tensorFinal->copy_(tensorEval);
//                }
//            }
//        }
//    }
//    torch::serialize::OutputArchive archive;
//    QFinal->save(archive);
//    archive.save_to(modelPath);
//
//    double endTime = thread_real_time();
//
//    cout << "Split model saved! Best epoch: " << bestEpoch << endl;
//    cout << "Split Model training time: " << endTime - startTime  << " seconds."<< endl;
//    delete cacheTree;
//}

void SplitLearner::trainR2R(const string& modelPath){
    double startTime = thread_real_time();
    int bestEpoch = -1, maxNonNegativeCnt = -1;
    size_t trainingSetSize = trainingSet_.size();
    vector<pair<torch::Tensor, int>> stateActionPair;
    auto *cacheTree = new RTree();
    DQN QFinal = DQN(SplitLearner::actionSpace_ * 4, 64, SplitLearner::actionSpace_);
    for(int epoIDX = 0; epoIDX < SplitLearner::epoch_; ++epoIDX){
        stepCnt_ = 0;
        int totalCnt = 0, positiveCnt = 0, nonNegativeCnt = 0;
        for(int partIDX = 0; partIDX < SplitLearner::partNum_; ++partIDX){
            double ratioForTreeBuild = 1.0 * (partIDX + 1) / (SplitLearner::partNum_);
            auto buildNum = static_cast<size_t>(ratioForTreeBuild * trainingSetSize);

            tree_->clearTree();
            referenceTree_->clearTree();
            cacheTree->clearTree();
            // Part 1: build the tree
            for(size_t rectIDX = 0; rectIDX < buildNum; ++rectIDX){
                tree_->insertRect(*trainingSet_[rectIDX]);
            }

            // Part 2: identify rectangles that will trigger splitting to the tree from Part 1
            vector<Rectangle*> rectForTrain;
            size_t rectForTrainNum = 0;
            for(size_t rectIDX = buildNum; rectIDX < trainingSetSize; ++rectIDX){
                Rectangle* r = trainingSet_[rectIDX];
                // traverse a path to a specific leaf node using function chooseSubtree but do not update the MBR
                TreeNode *node = tree_->treeNodes_[tree_->root_];
                // choose a child node of the internal node to insert and update the bound
                while(node->height_ > 0)
                    node = tree_->chooseSubtree(r, node);
                bool isOverflowed = node->entryNum_ + 1 > TreeNode::maxEntry_;
                if(isOverflowed){
                    rectForTrain.push_back(r);
                    ++rectForTrainNum;
                }else{ // if r do not trigger splitting to the tree, update the MBR back to the root along the insertion path
                    auto rect = tree_->createRect(r->low_, r->top_);
                    node->addEntry(rect->rectID_);
                    node->merge(*rect);
                    while (node->father_ >= 0){
                        node = tree_->treeNodes_[node->father_];
                        node->merge(*rect);
                    }
                    assert(node->height_ == tree_->height_ - 1);
                }
            }
            cacheTree->copyTree(tree_);
            referenceTree_->copyTree(tree_);

            // Part 3: Training
            int period = 0;
            vector<Rectangle*> rectForReward;
            for(size_t trainIDX = 0; trainIDX < rectForTrainNum; ++trainIDX){
                referenceTree_->insertRect(*rectForTrain[trainIDX]);

                Rectangle* r = rectForTrain[trainIDX];
                auto rect = tree_->createRect(r->low_, r->top_);
                // traverse a path to a specific leaf node using function chooseSubtree
                TreeNode *node = tree_->treeNodes_[tree_->root_];
                node->merge(*rect);
                // choose a child node of the internal node to insert and update the bound
                while(node->height_ > 0){
                    node = tree_->chooseSubtree(rect, node);
                    node->merge(*rect);
                }
                bool isOverflowed = node->addEntry(rect->rectID_);
                bool isTrigger = isOverflowed;
                bool isInLoop = false;
                while(isOverflowed){
                    vector<SplitLocation> splitLocations((TreeNode::maxEntry_ - 2 * TreeNode::minEntry_ + 2) * 2 * DIM);
                    vector<int>& entryIDList = node->entries_;

                    int loc = 0;
                    for(int dim = 0; dim < DIM; ++dim){
                        for(int splitDis : {0, 1}) {
                            if (splitDis == 0) { // sort by lower bound
                                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                                    if (node->height_ == 0) return tree_->objects_[idx1]->low_[dim] < tree_->objects_[idx2]->low_[dim];
                                    else return tree_->treeNodes_[idx1]->low_[dim] < tree_->treeNodes_[idx2]->low_[dim];
                                });
                            } else { // sort by upper bound
                                sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                                    if (node->height_ == 0)
                                        return tree_->objects_[idx1]->top_[dim] < tree_->objects_[idx2]->top_[dim];
                                    else
                                        return tree_->treeNodes_[idx1]->top_[dim] < tree_->treeNodes_[idx2]->top_[dim];
                                });
                            }

                            Rectangle recPrefix = tree_->mergeRange(node, 0, TreeNode::minEntry_ - 1);
                            Rectangle recSuffix = tree_->mergeRange(node, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1, node->entryNum_);
                            for (int candidate = TreeNode::minEntry_ - 1; candidate < TreeNode::maxEntry_ - TreeNode::minEntry_ + 1; ++candidate) {
                                if (node->height_ == 0)
                                    recPrefix.merge(*(tree_->objects_[entryIDList[candidate]]));
                                else
                                    recPrefix.merge(*(tree_->treeNodes_[entryIDList[candidate]]));
                                Rectangle recRemaining(recSuffix);
                                recRemaining.merge(tree_->mergeRange(node, candidate + 1, TreeNode::maxEntry_ - TreeNode::minEntry_ + 1));
                                splitLocations[loc].perimeter1_ = max(recPrefix.getPerimeter(), recRemaining.getPerimeter());
                                splitLocations[loc].perimeter2_ = min(recPrefix.getPerimeter(), recRemaining.getPerimeter());
                                splitLocations[loc].volume1_ = max(recPrefix.getVolume(), recRemaining.getVolume());
                                splitLocations[loc].volume2_ = min(recPrefix.getVolume(), recRemaining.getVolume());
                                splitLocations[loc].overlap_ = splitByOverlap(recPrefix, recRemaining);
                                splitLocations[loc].location_ = candidate;
                                splitLocations[loc].dimension_ = dim * 2 + splitDis;
                                ++loc;
                            }
                        }
                    }
                    assert(loc == (TreeNode::maxEntry_ - 2 * TreeNode::minEntry_ + 2) * 2 * DIM);

                    int nonOverlapSplitNum = 0;
                    for(auto& splitLocation : splitLocations){
                        if(splitLocation.overlap_ == 0) ++nonOverlapSplitNum;
                    }

                    int splitLoc = 0;
                    int splitDim = 0;
                    if(nonOverlapSplitNum > 1){
                        isInLoop = true;
                        torch::NoGradGuard no_grad;
                        int stateShape = SplitLearner::actionSpace_ * 4;
                        vector<double> states(stateShape);
                        vector<int> candidateSplitAction(SplitLearner::actionSpace_);
                        vector<pair<double, int>> zeroOverlapSplits;
                        for(int idx = 0; idx < splitLocations.size(); ++idx){
                            if(splitLocations[idx].overlap_ == 0){
                                double perimeter = max(splitLocations[idx].perimeter1_, splitLocations[idx].perimeter2_);
                                zeroOverlapSplits.emplace_back(perimeter, idx);
                            }
                        }
                        sort(zeroOverlapSplits.begin(), zeroOverlapSplits.end());
                        double maxArea = -DBL_MAX, minArea = DBL_MAX, maxPerimeter = -DBL_MAX, minPerimeter = DBL_MAX;

                        for(int idx = 0; idx < SplitLearner::actionSpace_; ++idx){
                            int splitIDX = zeroOverlapSplits[idx].second;
                            candidateSplitAction[idx] = splitIDX;
                            states[idx * 4] = splitLocations[splitIDX].volume1_;
                            states[idx * 4 + 1] = splitLocations[splitIDX].volume2_;
                            states[idx * 4 + 2] = splitLocations[splitIDX].perimeter1_;
                            states[idx * 4 + 3] = splitLocations[splitIDX].perimeter2_;
                            maxArea = max(maxArea, states[idx * 4]);
                            minArea = min(minArea, states[idx * 4 + 1]);
                            maxPerimeter = max(maxPerimeter, states[idx * 4 + 2]);
                            minPerimeter = min(minPerimeter, states[idx * 4 + 3]);
                        }
                        for(int idx = 0; idx < SplitLearner::actionSpace_; ++idx){
                            states[idx * 4] = (states[idx * 4] - minArea) / (maxArea - minArea + 0.001);
                            states[idx * 4 + 1] = (states[idx * 4 + 1] - minArea) / (maxArea - minArea + 0.001);
                            states[idx * 4 + 2] = (states[idx * 4 + 2] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
                            states[idx * 4 + 3] = (states[idx * 4 + 3] - minPerimeter) / (maxPerimeter - minPerimeter + 0.001);
                        }

                        auto stateTensor = torch::tensor(states, torch::kFloat32);
                        int action = -1;
                        if(trainIDX < SplitLearner::teacherForcing_ * rectForTrainNum){
                            auto minActionPerimeter = DBL_MAX;
                            for(int actionIDX = 0; actionIDX < SplitLearner::actionSpace_; ++actionIDX){
                                if(states[actionIDX * 4 + 2] + states[actionIDX * 4 + 3] < minActionPerimeter){
                                    minActionPerimeter = states[actionIDX * 4 + 2] + states[actionIDX * 4 + 3];
                                    action = actionIDX;
                                }
                            }
                        }else{
                            action = chooseAction(stateTensor);
                        }
                        assert(action >= 0 && action < SplitLearner::actionSpace_);
                        assert(candidateSplitAction[action] >= 0 && candidateSplitAction[action] < splitLocations.size());
                        stateActionPair.emplace_back(stateTensor, action);
                        splitLoc = splitLocations[candidateSplitAction[action]].location_;
                        splitDim = splitLocations[candidateSplitAction[action]].dimension_;
                    }else {
                        auto minOverlap = DBL_MAX;
                        for(auto& splitLocation : splitLocations){
                            if(splitLocation.overlap_ < minOverlap){
                                minOverlap = splitLocation.overlap_;
                                splitLoc = splitLocation.location_;
                                splitDim = splitLocation.dimension_;
                            }
                        }
                        if(isInLoop){
                            isInLoop = false;
                            torch::Tensor finalTensor = torch::zeros({actionSpace_ * 4}, torch::kFloat32);
                            stateActionPair.emplace_back(finalTensor, -1);
                        }
                    }

                    int splitDis = splitDim % 2;
                    splitDim /= 2;
                    if (splitDis == 0) { // sort by lower bound
                        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                            if (node->height_ == 0)
                                return tree_->objects_[idx1]->low_[splitDim] < tree_->objects_[idx2]->low_[splitDim];
                            else
                                return tree_->treeNodes_[idx1]->low_[splitDim] < tree_->treeNodes_[idx2]->low_[splitDim];
                        });
                    } else { // sort by upper bound
                        sort(entryIDList.begin(), entryIDList.end(), [&](const int &idx1, const int &idx2) -> bool {
                            if (node->height_ == 0)
                                return tree_->objects_[idx1]->top_[splitDim] < tree_->objects_[idx2]->top_[splitDim];
                            else
                                return tree_->treeNodes_[idx1]->top_[splitDim] < tree_->treeNodes_[idx2]->top_[splitDim];
                        });
                    }

                    vector<int> group1(entryIDList.begin(), entryIDList.begin() + splitLoc + 1);
                    vector<int> group2(entryIDList.begin() + splitLoc + 1, entryIDList.end());
                    Rectangle rect1(tree_->mergeRange(node, 0, splitLoc + 1));
                    Rectangle rect2(tree_->mergeRange(node, splitLoc + 1, node->entryNum_));


                    TreeNode *sibling = tree_->createNode();
                    sibling->height_ = node->height_;

                    node->copyEntries(group1);
                    node->setBound(rect1);
                    sibling->copyEntries(group2);
                    sibling->setBound(rect2);

                    if(sibling->height_ > 0){
                        for(int &entryID : sibling->entries_){
                            tree_->treeNodes_[entryID]->father_ = sibling->rectID_;
                        }
                        for(int &entryID : node->entries_){
                            tree_->treeNodes_[entryID]->father_ = node->rectID_;
                        }
                    }
                    if(node->father_ >= 0){
                        tree_->treeNodes_[node->father_]->addEntry(sibling->rectID_);
                        tree_->treeNodes_[node->father_]->merge(rect2);
                        sibling->father_ = node->father_;
                    } else {
                        TreeNode *newRoot = tree_->createNode();
                        newRoot->addEntry(node->rectID_);
                        newRoot->addEntry(sibling->rectID_);
                        newRoot->setBound(rect1);
                        newRoot->merge(rect2);
                        newRoot->height_ = tree_->height_++;

                        tree_->root_ = newRoot->rectID_;
                        node->father_ = newRoot->rectID_;
                        sibling->father_ = newRoot->rectID_;
                    }
                    node = tree_->treeNodes_[node->father_];
                    isOverflowed = node->entries_.size() > TreeNode::maxEntry_;
                }
                if(isTrigger && !stateActionPair.empty() && stateActionPair.back().second >= 0){
                    torch::Tensor finalTensor = torch::zeros({actionSpace_ * 4}, torch::kFloat32);
                    stateActionPair.emplace_back(finalTensor, -1);
                }

                if(!tree_->checkConsistency()){
                    cout << "epoIDX: " << epoIDX << ", partIDX: " << partIDX << ", trainIDX: " << trainIDX << endl;
                    throw std::runtime_error("tree is not well shaped.");
                }
                if(!referenceTree_->checkConsistency()){
                    cout << "epoIDX: " << epoIDX << ", partIDX: " << partIDX << ", trainIDX: " << trainIDX << endl;
                    throw std::runtime_error("referenceTree is not well shaped.");
                }

                rectForReward.push_back(r);
                if(++period % SplitLearner::rewardComFreq_ == 0){
                    float reward = computeDenseRewardR2R(rectForReward);
                    for(size_t ind = 0; ind+1 < stateActionPair.size(); ++ind){
                        if (stateActionPair[ind].second == -1) continue;
                        if (stateActionPair[ind+1].second == -1){
                            memory_.storeTransition(stateActionPair[ind].first, stateActionPair[ind].second,
                                                    reward, stateActionPair[ind+1].first, true);
                        }else{
                            memory_.storeTransition(stateActionPair[ind].first, stateActionPair[ind].second,
                                                    reward, stateActionPair[ind+1].first, false);
                        }
                    }
                    optimize();
                    referenceTree_->copyTree(cacheTree);
                    tree_->copyTree(cacheTree);
                    rectForReward.clear();
                    stateActionPair.clear();

                    if (reward > 0) positiveCnt++;
                    if (reward >= 0) nonNegativeCnt++;
                    totalCnt++;
                }
            }
            cout << "Epoch number: " << epoIDX << ", part index: " << partIDX << endl;
            cout << "Positive reward number: " << positiveCnt << " / " << totalCnt << endl;
            cout << "Non negative reward number: " << nonNegativeCnt << " / " << totalCnt << endl << endl;
        }
        if(nonNegativeCnt > maxNonNegativeCnt){
            maxNonNegativeCnt = nonNegativeCnt;
            bestEpoch = epoIDX;
            torch::NoGradGuard no_grad;
            auto paramsEval = QEval_->named_parameters(true);
            auto paramsFinal = QFinal->named_parameters(true);
            for (auto& val : paramsEval) {
                auto& name = val.key();
                auto& tensorEval = val.value();
                auto* tensorFinal = paramsFinal.find(name);
                if (tensorFinal != nullptr) {
                    tensorFinal->copy_(tensorEval);
                }
            }
        }
    }
    torch::serialize::OutputArchive archive;
    QFinal->save(archive);
    archive.save_to(modelPath);

    double endTime = thread_real_time();

    cout << "Split model saved! Best epoch: " << bestEpoch << endl;
    cout << "Split Model training time: " << endTime - startTime  << " seconds."<< endl;
    delete cacheTree;
}
