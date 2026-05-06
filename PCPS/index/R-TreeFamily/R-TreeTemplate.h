#ifndef PCPS_R_TREETEMPLATE_H
#define PCPS_R_TREETEMPLATE_H

#include "../IIndex.h"

enum EntryType{Query, Node};
struct ReInsertItem{
    EntryType entryType_;
    int itemID_;
};

class RTreeNode: public Rectangle{
public:
    int nodeID_ = -1;
    int entryNum_ = 0;
    int father_ = -1;
    int height_ = 0; // when height_ == 0, the node is a leaf node, otherwise, is internal node
    vector<int> entries_;
    vector<double> center_;

    static int maxEntry_;
    static int minEntry_;

    RTreeNode(): center_(DIM){};
    RTreeNode(int nodeID): nodeID_(nodeID), center_(DIM){}
    RTreeNode(const vector<double>& low, const vector<double>& top): center_(DIM){
        low_.assign(low.begin(), low.end());
        top_.assign(top.begin(), top.end());
        for(int dim = 0; dim < DIM; ++dim)
            center_[dim] = (low[dim] + top[dim]) * 0.5;
    }

    bool addEntry(int entryID);
    bool removeEntry(int entryID);
    void copyEntries(const vector<int>& entries);

    void setBound(const Rectangle& rect) override;
    void setBound(const vector<double>& low, const vector<double>& top) override;
};

class RTreeTemplate: public IIndex{
public:
    vector<RTreeNode*> treeNodes_{};
    queue<int> freeNodeID_{};
    int root_ = -1;
    int height_ = 0;

    RTreeTemplate() = default;
    ~RTreeTemplate() override{
        for(auto & treeNode : treeNodes_)
            delete treeNode;
    }

    RTreeNode* createNode();

    Rectangle mergeRange(RTreeNode* node, int startIDX, int endIDX);

    void registerCRQuery(CRQuery* query) override;
    void cancelCRQuery(CRQuery* query) final;
    void publishPoint(Point* point) final;
    bool checkConsistency() const final;

    virtual RTreeNode* chooseSubtree(Rectangle* rect, RTreeNode* node) = 0;
    virtual RTreeNode* splitNode(RTreeNode* node);
    virtual void partition(RTreeNode* node,
                           vector<int>& group1, vector<int>& group2, Rectangle& rect1, Rectangle& rect2) = 0;

    RTreeNode* findLeaf(CRQuery& query);
    void condenseTree(RTreeNode* node, vector<ReInsertItem>& reinsertList);
    virtual void reinsert(vector<ReInsertItem>& reinsertList);
};

#endif //PCPS_R_TREETEMPLATE_H
