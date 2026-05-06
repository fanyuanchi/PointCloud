#ifndef PCPS_R_TREE_H
#define PCPS_R_TREE_H

#include "../R-TreeTemplate.h"

enum SplitType{Quadratic, Linear, Greene, MinOverlap};

class RTree: public RTreeTemplate{
public:
    static SplitType splitType_;

    RTree() = default;
    ~RTree() override = default;

    RTreeNode* chooseSubtree(Rectangle* rect, RTreeNode* node) override;
    RTreeNode* splitNode(RTreeNode* node) override;

    void partitionQuadratic(RTreeNode* node,
                            vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2);
    void partitionLinear(RTreeNode* node,
                         vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2);
    void partitionGreene(RTreeNode* node,
                         vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2);
    void partitionMinOverlap(RTreeNode* node,
                             vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2);
    // empty invalid partition strategy
    void partition(RTreeNode* node,
                   vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2) override {}
};

#endif //PCPS_R_TREE_H
