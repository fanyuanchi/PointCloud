#ifndef PCPS_RLR_TREE_H
#define PCPS_RLR_TREE_H

#include "../R-TreeTemplate.h"
#include <torch/torch.h>

struct DQNImpl : torch::nn::Module {
    torch::nn::Linear linear1{nullptr};
    torch::nn::Linear linear2{nullptr};
    explicit DQNImpl(int inputDim = 8, int hiddenDim = 64, int outputDim = 2){
        linear1 = register_module("linear1", torch::nn::Linear(inputDim, hiddenDim));
        linear2 = register_module("linear2", torch::nn::Linear(hiddenDim, outputDim));
    }

    torch::Tensor forward(torch::Tensor x) {
        x = linear1(x);
        x = torch::selu(x);
        x = linear2(x);
        return x;
    }
};
TORCH_MODULE(DQN);

struct SplitLocation{
    double perimeter1;
    double perimeter2;
    double area1;
    double area2;
    double overlap;
    int location;
    int dimension; // split is dimension / 2 , low is dimension % 2 == 0, top is dimension % 2 == 1
};

class RLRTree: public RTreeTemplate{
public:
    DQN insertModel_;
    DQN splitModel_;

    static int actionSpace_;
    static string insertModelPath_;
    static string splitModelPath_;

    RLRTree() {
        insertModel_ = DQN(RLRTree::actionSpace_ * 4, 64, RLRTree::actionSpace_);
        splitModel_ = DQN(RLRTree::actionSpace_ * 4, 64, RLRTree::actionSpace_);

        torch::serialize::InputArchive insertArchive;
        insertArchive.load_from(RLRTree::insertModelPath_);
        insertModel_->load(insertArchive);
        insertModel_->eval();

        torch::serialize::InputArchive splitArchive;
        splitArchive.load_from(RLRTree::splitModelPath_);
        splitModel_->load(splitArchive);
        splitModel_->eval();
    }
    ~RLRTree() override = default;


    RTreeNode* chooseSubtree(Rectangle* rect, RTreeNode* node) override;
    void partition(RTreeNode* node, vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2) override;

};

#endif //PCPS_RLR_TREE_H
