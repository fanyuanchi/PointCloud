#ifndef RLR_TREETRAIN_R_TREE_H
#define RLR_TREETRAIN_R_TREE_H

#include "Geometry.h"

enum TrainType{ChooseSubtree, SplitNode};
enum Dataset{Gaussian, RiverBank, Mountain};
extern unordered_map<enum TrainType, string> trainNames;
extern unordered_map<enum Dataset, string> datasetNames;

double splitByVolume(const Rectangle& rect1, const Rectangle& rect2);
double splitByOverlap(const Rectangle& rect1, const Rectangle& rect2);

struct SplitLocation{
    double perimeter1_;
    double perimeter2_;
    double volume1_;
    double volume2_;
    double overlap_;
    int location_;
    int dimension_; // split is dimension / 2 , low is dimension % 2 == 0, top is dimension % 2 == 1
};

class TreeNode: public Rectangle{
public:
    int entryNum_ = 0;
    int father_ = -1;
    int height_ = 0; // when height_ == 0, the node is a leaf node, otherwise, is internal node
    vector<int> entries_;

    static int maxEntry_;
    static int minEntry_;

    TreeNode() = default;
    TreeNode(const vector<double>& low, const vector<double>& top){
        low_.assign(low.begin(), low.end());
        top_.assign(top.begin(), top.end());
    }
    explicit TreeNode(TreeNode* node){
        entryNum_ = node->entryNum_;
        father_ = node->father_;
        height_ = node->height_;
        entries_.assign(node->entries_.begin(), node->entries_.end());
        for(int dim = 0; dim < DIM; ++dim){
            low_[dim] = node->low_[dim];
            top_[dim] = node->top_[dim];
        }
        rectID_ = node->rectID_;
    }

    bool addEntry(int entryID);
    void copyEntries(const vector<int>& entries);
};

class RTree{
public:
    vector<Rectangle*> objects_{};
    vector<TreeNode*> treeNodes_{};
    int root_ = -1;
    int height_ = 0;
    int resNum_ = 0;

    RTree() = default;
    ~RTree(){
        for (auto& object : objects_) {
            delete object;
        }
        for(auto& treeNode : treeNodes_)
            delete treeNode;
    }

    TreeNode* createNode();
    Rectangle* createRect(const vector<double>& low, const vector<double>& top);
    void copyTree(RTree* tree);
    void clearTree();

    Rectangle mergeRange(TreeNode* node, int startIDX, int endIDX);

    void insertRect(Rectangle& rect);
    virtual TreeNode* chooseSubtree(Rectangle* rect, TreeNode* node);
    TreeNode* splitNode(TreeNode* node);
    virtual void partition(TreeNode* node, vector<int>& group1, vector<int>& group2, Rectangle& rect1, Rectangle& rect2);

    int stabbingQuery(Point& point);
    double accessRateStabbing(Point& point);

    int rangeQuery(Rectangle &range);
    double accessRateRange(Rectangle &range);

    bool checkConsistency();
};

#endif //RLR_TREETRAIN_R_TREE_H
