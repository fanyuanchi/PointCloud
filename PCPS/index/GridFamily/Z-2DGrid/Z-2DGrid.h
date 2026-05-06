#ifndef PCPS_Z_2DGRID_H
#define PCPS_Z_2DGRID_H

#include "../../IIndex.h"

class Cell2D {
public:
    vector<int> fullCover_{};
    vector<int> partCover_{};
};

class Grid2D {
public:
    vector<Cell2D> cells_;

    Grid2D(int curG): cells_(curG * curG) {}

    static int gMin_, gMax_;
};

class TreeNode {
public:
    double zMin_, zMax_, zMid_;
    int height_ = -1;
//    size_t nodeRegCounter_ = 0;
    vector<int> leafList_{};
    Grid2D* gridIndex_ = nullptr;
    TreeNode* leftChild_ = nullptr;
    TreeNode* rightChild_ = nullptr;

    // initialize root node
    TreeNode(const double zMin, const double zMax): zMin_(zMin), zMax_(zMax), height_(0) {
        zMid_ = (zMin_ + zMax_) * 0.5;
    }
    // initialize a leaf node
    TreeNode(const TreeNode* father, const bool isLeft) {
        if (isLeft) {
            zMin_ = father->zMin_; zMax_ = father->zMid_;
        }else {
            zMax_ = father->zMax_; zMin_ = father->zMid_;
        }
        zMid_ = (zMin_ + zMax_) * 0.5;
        height_ = father->height_ + 1;
    }
    ~TreeNode(){
        delete leftChild_;
        delete rightChild_;
        delete gridIndex_;
    }
};

class Z2DGrid: public IIndex{
public:
    int delNum_ = 0;
    int curG_ = Grid2D::gMax_;
    double low_[3]{};
    double top_[3]{};
    TreeNode *root_ = nullptr;
    int curAliveQueryNum_ = 0;

    static int hMax_, rMin_;
    static double ratioForDelete_;
    static double ratioForSplit_;

    Z2DGrid(vector<double> LOW, vector<double> TOP) {
        if(LOW.size() != 3 || TOP.size() != 3 || DIM != 3){
            throw std::runtime_error("invalid dimensionality");
        }
        for(int dim = 0; dim < 3; ++dim){
            low_[dim] = LOW[dim];
            top_[dim] = TOP[dim];
        }
        root_ = new TreeNode(low_[2], top_[2]);
    }

    void registerCRQuery(CRQuery* query) override;
    void cancelCRQuery(CRQuery* query) override;
    void publishPoint(Point* point) override;
    bool checkConsistency() const override;
    void condenseTree(double curTime) override;

    void splitNode(TreeNode* node);
    void insertQuery(TreeNode* node, CRQuery* query);
    int getCellRanges(CRQuery* query, vector<pair<size_t,size_t>>& ranges);

    void rebuild();
    void fullScan2Del();
};

#endif //PCPS_Z_2DGRID_H
