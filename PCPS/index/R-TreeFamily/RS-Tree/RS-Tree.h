#ifndef PCPS_RS_TREE_H
#define PCPS_RS_TREE_H

#include "../R-TreeTemplate.h"

class RSTree: public RTreeTemplate{
public:
    static double ratioForReinsert_;

    RSTree() = default;
    ~RSTree() override = default;


    void registerCRQuery(CRQuery* query) override;

    RTreeNode* chooseSubtree(Rectangle* rect, RTreeNode* node) override;

    void forcedReinsert(RTreeNode* node, unordered_set<int>& curReinsert);
    void reinsertEntry(ReInsertItem item, unordered_set<int>& curReinsert);

    void partition(RTreeNode* node,
                   vector<int> &group1, vector<int> &group2, Rectangle& rect1, Rectangle& rect2) override;

    void reinsert(vector<ReInsertItem>& reinsertList) override;
};

#endif //PCPS_RS_TREE_H
