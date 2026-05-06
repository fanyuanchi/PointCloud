#ifndef RLR_TREETRAIN_NEURALNET_H
#define RLR_TREETRAIN_NEURALNET_H

#include <torch/torch.h>
#include <utility>
#include "random"
#include "vector"

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


struct Transition {
    torch::Tensor state_;
    int action_;
    float reward_;
    torch::Tensor nextState_;
    bool isDone_;

    Transition(const torch::Tensor& state, int action, float reward, const torch::Tensor& nextState, bool isDone):
            state_(state), action_(action), reward_(reward), nextState_(nextState), isDone_(isDone) {}
};

class ReplayMemory{
public:
    std::vector<Transition> memory_;
    int position_ = 0;
    static int capacity_;
    static int batchSize_;

    ReplayMemory() = default;
    explicit ReplayMemory(int capacity){
        capacity_ = capacity;
    }

    void storeTransition(const torch::Tensor& state, int action, float reward, const torch::Tensor& nextState, bool isDone);
    std::vector<Transition> sampleTransition(std::mt19937& rng_);
    [[nodiscard]] size_t curSize() const{ return memory_.size();}
};

#endif //RLR_TREETRAIN_NEURALNET_H
