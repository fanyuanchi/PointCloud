#ifndef PCPS_R_BVH_H
#define PCPS_R_BVH_H
#include "../IIndex.h"

enum RegType{CCR, LBR};
enum PubType{PSU, RSU};
enum CanType{ABD, CBD};

// Invariant:
// Each queryID appears at most once in a node,
// either in fullCover_ or in partCover_, never both.
class RBVHNode: public Rectangle{
public:
    bool inFlush_ = false;
    int buffer_ = 0;
    static int maxB_;
    vector<int> fullCover_;
    vector<int> partCover_;
    RBVHNode** children_;

    RBVHNode(int subCode, RBVHNode *parent){
        low_.resize(DIM);
        top_.resize(DIM);
        for(int dim = DIM-1; dim >= 0; --dim){
            if(subCode & 1) {
                low_[dim] = (parent->low_[dim] + parent->top_[dim]) / 2.0;
                top_[dim] = parent->top_[dim];
            }
            else{
                low_[dim] = parent->low_[dim];
                top_[dim] = (parent->low_[dim] + parent->top_[dim]) / 2.0;
            }
            subCode >>= 1;
        }
        children_ = nullptr;
    }

    RBVHNode(const vector<double>& LOW, const vector<double>& TOP){
        low_.assign(LOW.begin(), LOW.end());
        top_.assign(TOP.begin(), TOP.end());
        children_ = nullptr;
    }

    ~RBVHNode(){
        if(children_ != nullptr){
            int childNum = 1 << DIM;
            for(int idx = 0; idx < childNum; ++idx)
                delete children_[idx];
            delete []children_;
        }
    }
};

struct STCell {
    double intensity_ = 0.0;
    double lastTime_ = DBL_MAX;
};

class RBVH: public IIndex{
public:
    queue<RBVHNode*> flushQ_;
    RBVHNode *root_ = nullptr;
    int curMaxHeight_;
    int curAliveQueryNum = 0;
    double ratioForSplit_;
    vector<double> cellLength_{};
    vector<vector<STCell>> lastSTVec_;
    vector<vector<STCell>> curSTVec_;
    map<float, int> flushMap_{};

    static int minH_, maxH_;
    static double minR_, maxR_;
    static double lambda_, kappa_, tau_;

    RBVH(const vector<double>& LOW, const vector<double>& TOP){
        assert(LOW.size() == DIM && TOP.size() == DIM);
        curMaxHeight_ = RBVH::maxH_;
        ratioForSplit_ = RBVH::minR_;
        root_ = new RBVHNode(LOW, TOP);
        cellLength_.resize(DIM);
        lastSTVec_.resize(DIM);
        curSTVec_.resize(DIM);
        for(int dim = 0; dim < DIM; ++dim){
            cellLength_[dim] = (TOP[dim] - LOW[dim]) / 256.0;
            lastSTVec_[dim].resize(256);
            curSTVec_[dim].resize(256);
        }
    }

    ~RBVH() override{
        delete root_;
    }

    void registerCRQuery(CRQuery* query) override;
    void cancelCRQuery(CRQuery* query) override;
    void publishPoint(Point* point) override;
    bool checkConsistency() const override;

    void flush() override;
    void condenseTree(double curTime) override;
    void updateSTList(const Point* p, double curTime) override;
    float getFlushInterval() override;

    bool incrementalConstruction(Point* point) override;
    void publishSynchronizedDelete(Point* point) override;

    void splitNode(RBVHNode *node);
    void publishFull(RBVHNode *node);
    void publishPart(RBVHNode *node, Point* p);
    void rollBack2Base(RBVHNode *node, int height);
    bool cutHair(RBVHNode *node, int height);
    void cutHead(RBVHNode *node, int height);
    void rebuild();
    bool isDrift(double curTime);
};

#endif //PCPS_R_BVH_H
