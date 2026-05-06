#include <random>
#include "NeuralNet.h"

int ReplayMemory::capacity_ = 5000;
int ReplayMemory::batchSize_ = 64;

void ReplayMemory::storeTransition(const torch::Tensor& state, int action, float reward, const torch::Tensor& nextState, bool isDone){
    if(memory_.size() < ReplayMemory::capacity_) {
        memory_.emplace_back(state, action, reward, nextState, isDone);
    } else {
        memory_[position_].state_ = state;
        memory_[position_].action_ = action;
        memory_[position_].reward_ = reward;
        memory_[position_].nextState_ = nextState;
        memory_[position_].isDone_ = isDone;
    }
    position_ = (position_+1) % ReplayMemory::capacity_;
}

std::vector<Transition> ReplayMemory::sampleTransition(std::mt19937& rng_){
    std::vector<Transition> batch;
    std::sample(memory_.begin(), memory_.end(),
                std::back_inserter(batch),
                ReplayMemory::batchSize_,
                rng_);
    return batch;
}