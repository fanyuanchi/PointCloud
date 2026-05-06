#include "InsertLearner.h"

double InsertLearner::ALPHA_ = 0.003; // 0.003
double InsertLearner::GAMMA_ = 0.95;  // 0.95
double InsertLearner::EPSILON_ = 1.0; // 1.0
double InsertLearner::EPS_END_ = 0.1;
int InsertLearner::actionSpace_ = 2;
int InsertLearner::replaceCnt_ = 50;
int InsertLearner::epoch_ = 20;
int InsertLearner::rewardComFreq_ = 10;

int InsertLearner::chooseAction(const torch::Tensor& insertState){
    torch::NoGradGuard no_grad;
    if(++stepCnt_ > 400000) { // 400000
        if(EPSILON_ > EPS_END_) EPSILON_ *= 0.99;
        else EPSILON_ = EPS_END_;
    }
    double rand = uniform01_(rng_);
    if(rand < EPSILON_) {
        return actionDist_(rng_);
    }
    auto actions = QEval_->forward(insertState.unsqueeze(0));
    return actions.argmax(1).item<int>();
}

void InsertLearner::optimize(){
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

    auto rewardTensor = torch::tensor(rewards).to(torch::kFloat32).unsqueeze(1);
    auto doneTensor= torch::tensor(dones).unsqueeze(1);
    auto Qtarget = rewardTensor + GAMMA_ * maxNext * (1.0 - doneTensor);

    optimizer_->zero_grad();
    auto loss = loss_(QpredChosen, Qtarget.detach());
    loss.backward();
    optimizer_->step();
}

float InsertLearner::computeDenseRewardP2R(int curIDX){
    double avgAccessRate = 0;
    for(int rectIDX = 0; rectIDX < InsertLearner::rewardComFreq_; ++rectIDX){
        auto rect = trainingSet_[curIDX-rectIDX];
        Point point = rect->getCenter();
        double referenceRate = referenceTree_->accessRateStabbing(point);
        double treeRate = tree_->accessRateStabbing(point);
        avgAccessRate += referenceRate - treeRate;
    }
    return static_cast<float>(avgAccessRate / InsertLearner::rewardComFreq_);
}

float InsertLearner::computeDenseRewardR2R(int curIDX){
    double avgAccessRate = 0;
    for(int rectIDX = 0; rectIDX < InsertLearner::rewardComFreq_; ++rectIDX){
        Point center = trainingSet_[curIDX-rectIDX]->getCenter();
        Rectangle queryRange;
        //TODO JUST TRY
        DataLoader::generateOneSampledRect(center, queryRange, 1e-4, 1e-4, rng_);
        double referenceRate = referenceTree_->accessRateRange(queryRange);
        double treeRate = tree_->accessRateRange(queryRange);
        assert(tree_->resNum_ == referenceTree_->resNum_);
        avgAccessRate += referenceRate - treeRate;
    }
    return static_cast<float>(avgAccessRate / InsertLearner::rewardComFreq_);
}

void InsertLearner::trainR2R(const string& modelPath){
    double startTime = thread_real_time();
    int bestEpoch = -1, maxNonNegativeCnt = -1;
    size_t trainingSetSize = trainingSet_.size();
    vector<pair<torch::Tensor, int>> stateActionPair;
    DQN QFinal = DQN(InsertLearner::actionSpace_ * 4, 64, InsertLearner::actionSpace_);

    for(int epoIDX = 0; epoIDX < InsertLearner::epoch_; ++epoIDX){
        int totalCnt = 0, positiveCnt = 0, nonNegativeCnt = 0;
        tree_->clearTree();
        referenceTree_->clearTree();
        for(size_t rectIDX = 0; rectIDX < trainingSetSize; ++rectIDX){
            Rectangle* r = trainingSet_[rectIDX];
            // insert rect into the reference tree (standard R-Tree)
            referenceTree_->insertRect(*r);

            // insert rect into the trained tree (RLR-Tree)
            auto rect = tree_->createRect(r->low_, r->top_);
            // if tree is empty, create a root node and set the bound
            if(tree_->root_ < 0){
                auto* newRoot = tree_->createNode();
                newRoot->setBound(*rect);
                newRoot->addEntry(rect->rectID_);
                tree_->height_ = 1;
                tree_->root_ = 0;
                continue;
            }

            // if tree is not empty, traverse a path to a specific leaf node
            TreeNode *node = tree_->treeNodes_[tree_->root_];
            node->merge(*rect);

            // choose a child node of the internal node to insert and update the bound
            while(node->height_ > 0){
                TreeNode* chosenNode = nullptr;
                vector<TreeNode*> COV;
                for(int& nodeID : node->entries_){
                    if(tree_->treeNodes_[nodeID]->isContain(*rect)){
                        COV.push_back(tree_->treeNodes_[nodeID]);
                    }
                }
                if(!COV.empty()){ // if any child contains #rect
                    auto minVolume = DBL_MAX;
                    for(auto nodePtr: COV){
                        double volume = nodePtr->getVolume();
                        if(volume < minVolume){
                            minVolume = volume;
                            chosenNode = nodePtr;
                        }
                    }
                }else{
                    torch::NoGradGuard no_grad;
                    vector<int> entryIDList = node->entries_;
                    sort(entryIDList.begin(), entryIDList.end(), [&](const int& idx1, const int& idx2) -> bool {
                        return tree_->treeNodes_[idx1]->getMerge(*rect).getVolume() - tree_->treeNodes_[idx1]->getVolume()
                               < tree_->treeNodes_[idx2]->getMerge(*rect).getVolume() - tree_->treeNodes_[idx2]->getVolume();
                    });

                    int stateShape = InsertLearner::actionSpace_ * 4;
                    vector<double> states(stateShape, 0.0);
                    double volumeNormalize = -DBL_MAX, marginNormalize = -DBL_MAX;
                    double overlapNormalize = -DBL_MAX, occupancyNormalize = -DBL_MAX;

                    for(int idx1 = 0; idx1 < InsertLearner::actionSpace_; ++idx1){
                        TreeNode* curNode = tree_->treeNodes_[entryIDList[idx1]];
                        int loc = idx1 * 4;
                        Rectangle newRect(curNode->getMerge(*rect));
                        states[loc] = newRect.getVolume() - curNode->getVolume();
                        states[loc+1] = newRect.getPerimeter() - curNode->getPerimeter();
                        double oldOverlap = 0.0, newOverlap = 0.0;
                        for(int idx2 = 0; idx2 < node->entryNum_; ++idx2){
                            if(idx1 == idx2) continue;
                            TreeNode* otherNode = tree_->treeNodes_[entryIDList[idx2]];
                            oldOverlap += splitByOverlap(*curNode, *otherNode);
                            newOverlap += splitByOverlap(newRect, *otherNode);
                        }
                        states[loc+2] = newOverlap - oldOverlap;
                        states[loc+3] = 1.0 * curNode->entryNum_ / TreeNode::maxEntry_;

                        volumeNormalize = max(volumeNormalize, states[loc]);
                        marginNormalize = max(marginNormalize, states[loc+1]);
                        overlapNormalize = max(overlapNormalize, states[loc+2]);
                        occupancyNormalize = max(occupancyNormalize, states[loc+3]);
                    }
                    for(int idx = 0; idx < InsertLearner::actionSpace_; ++idx){
                        states[idx * 4] = states[idx * 4] / (volumeNormalize + 0.001);
                        states[idx * 4 + 1] = states[idx * 4 + 1] / (marginNormalize + 0.001);
                        states[idx * 4 + 2] = states[idx * 4 + 2] / (overlapNormalize + 0.001);
                        states[idx * 4 + 3] = states[idx * 4 + 3] / (occupancyNormalize + 0.001);
                    }

                    auto stateTensor = torch::tensor(states, torch::kFloat32);
                    int action = chooseAction(stateTensor);

                    stateActionPair.emplace_back(stateTensor, action);
                    chosenNode = tree_->treeNodes_[entryIDList[action]];
                }
                node = chosenNode;
                node->merge(*rect);
            }
            torch::Tensor finalTensor = torch::zeros({InsertLearner::actionSpace_ * 4}, torch::kFloat32);
            stateActionPair.emplace_back(finalTensor, -1);
            // split the node if overflowed and callback along the traverse path to update the tree
            bool isOverflowed = node->addEntry(rect->rectID_);
            while(isOverflowed){
                node = tree_->splitNode(node);
                isOverflowed = node->entries_.size() > TreeNode::maxEntry_;
            }

            if ((rectIDX+1) > TreeNode::maxEntry_ && (rectIDX+1) % InsertLearner::rewardComFreq_ == 0){
                float reward = computeDenseRewardR2R(static_cast<int>(rectIDX));

                if (reward > 0) positiveCnt++;
                if (reward >= 0) nonNegativeCnt++;
                totalCnt++;

                for(size_t ind = 0; ind+1 < stateActionPair.size(); ++ind){
                    if(stateActionPair[ind].second == -1) continue;
                    if(stateActionPair[ind+1].second == -1){
                        memory_.storeTransition(stateActionPair[ind].first, stateActionPair[ind].second,
                                                reward, stateActionPair[ind+1].first, true);
                    }else{
                        memory_.storeTransition(stateActionPair[ind].first, stateActionPair[ind].second,
                                                reward, stateActionPair[ind+1].first, false);
                    }

//                    cout << "state: " << stateActionPair[ind].first << endl;
//                    cout << "action: " << stateActionPair[ind].second << endl;
//                    cout << "reward: " << reward << endl;
//                    cout << "nextState: " << stateActionPair[ind+1].first << endl << endl;
                }
                stateActionPair.clear();
                optimize();
                referenceTree_->copyTree(tree_);
            }
            if ((rectIDX+1) % 5000 == 0){
                cout << "Epoch number: " << epoIDX << ", training rects: " << rectIDX+1 << endl;
                cout << "Positive reward number: " << positiveCnt << " / " << totalCnt << endl;
                cout << "Non negative reward number: " << nonNegativeCnt << " / " << totalCnt << endl;

                cout << "Tree height: " << tree_->height_ << endl;
                cout << "Epsilon: " << EPSILON_ << endl;
                cout << "StepCnt: " << stepCnt_ << endl;
                cout << "Memory Size: " << memory_.curSize() << endl << endl;
            }
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

    cout << "Insert model saved! Best epoch: " << bestEpoch << endl;
    cout << "Insertion Model training time: " << endTime - startTime  << " seconds."<< endl;
}
