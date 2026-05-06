#ifndef PCPS_RRS_TREE_H
#define PCPS_RRS_TREE_H

#include "../R-TreeTemplate.h"

class RRSTree: public RTreeTemplate{
public:
    static double RRs_;
    static double y1_;
    static double ys_;

    RRSTree() = default;
    ~RRSTree() override = default;
    double getDeltaOverlappedVolume(RTreeNode* node, int targetIDX, int otherIDX, Rectangle* rect) const;
    double getDeltaOverlappedPerimeter(RTreeNode* node, int targetIDX, int otherIDX, Rectangle* rect) const;
    double getDeltaOverlappedPerimeter(RTreeNode* node, int targetIDX, int startIDX, int endIDX, Rectangle* rect) const;

    RTreeNode* chooseSubtree(Rectangle* rect, RTreeNode* node) override;
    void CheckComp(unordered_set<int>& CAND, vector<double>& ovlp, RTreeNode* node,
                   int targetIDX, Rectangle* rect, bool usePerimeter, int priority, bool& success, int& candidate) const;

    void partition(RTreeNode* node,
                   vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2) override;
};


#endif //PCPS_RRS_TREE_H
