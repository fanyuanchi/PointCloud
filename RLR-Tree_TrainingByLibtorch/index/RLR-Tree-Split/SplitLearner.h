#ifndef RLR_TREETRAIN_SPLITLEARNER_H
#define RLR_TREETRAIN_SPLITLEARNER_H

#include <torch/torch.h>
#include <utility>
#include "../R-Tree.h"
#include "../../dataLoader/DataLoader.h"
#include "../NeuralNet.h"

class SplitLearner {
public:
    explicit SplitLearner(int seed = 17, enum Dataset dataset = Dataset::Gaussian):rng_(seed),
            actionDist_(0, SplitLearner::actionSpace_-1){
        QEval_ = DQN(SplitLearner::actionSpace_ * 4, 64, SplitLearner::actionSpace_);
        QNext_ = DQN(SplitLearner::actionSpace_ * 4, 64, SplitLearner::actionSpace_);
        optimizer_ = std::make_unique<torch::optim::RMSprop>(QEval_->parameters(),
                                                             torch::optim::RMSpropOptions(SplitLearner::ALPHA_));
        loss_ = torch::nn::MSELoss();

        referenceTree_ = new RTree();
        tree_ = new RTree();

        dataLoader_ = make_unique<DataLoader>(seed+47);
        if(dataset == Dataset::Gaussian){
            DIM = 3;
            DataLoader::low_ = {0, 0, 0};
            DataLoader::top_ = {1000, 1000, 1000};
            dataLoader_->loadGaussianRect(trainingSet_);
        }else{
            DIM = 3;
            string pointPath;
            if(dataset == Dataset::RiverBank){
                pointPath = "/home/data/fanyuanchi/PointCloud/RiverBank.csv";
                DataLoader::low_ = {-340, 60, -9.5};
                DataLoader::top_ = {-175, 220, 6.5};
            }else{
                pointPath = "/home/data/fanyuanchi/PointCloud/Mountain.csv";
                DataLoader::low_ = {-60  , -73.5, -2};
                DataLoader::top_ = { 75.5,  77.5,  48.5};
            }
            dataLoader_->loadSampleRect(trainingSet_, pointPath);
        }
    }
    ~SplitLearner(){
        delete referenceTree_;
        delete tree_;
        for(auto& rect: trainingSet_)
            delete rect;
    }

    int chooseAction(const torch::Tensor& insertState);
    void optimize();
    void trainP2R(const string& modelPath);
    void trainR2R(const string& modelPath);

    float computeDenseRewardP2R(vector<Rectangle*>& rectForReward);
    float computeDenseRewardR2R(vector<Rectangle*>& rectForReward);

public:
    static double ALPHA_;
    static double GAMMA_;
    static double EPSILON_;
    static double EPS_END_;
    static double teacherForcing_;
    static int partNum_;
    static int actionSpace_;
    static int replaceCnt_;
    static int epoch_;
    static int rewardComFreq_;

    DQN QEval_{nullptr};
    DQN QNext_{nullptr};
    std::unique_ptr<torch::optim::RMSprop> optimizer_;
    torch::nn::MSELoss loss_;

    int stepCnt_ = 0;
    int learnStepCnt_ = 0;

    ReplayMemory memory_ = ReplayMemory();
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform01_{0.0, 1.0};
    std::uniform_int_distribution<int> actionDist_;

    RTree* referenceTree_;
    RTree* tree_;

    vector<Rectangle*> trainingSet_{};
    unique_ptr<DataLoader> dataLoader_;
};


#endif //RLR_TREETRAIN_SPLITLEARNER_H
