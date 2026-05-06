#ifndef R_BVH_TMP_R_BVH_H
#define R_BVH_TMP_R_BVH_H

#include "Geometry.h"

struct Result{
    vector<int> resList_;
    size_t total_;
    Result(): resList_(9, 0), total_(0){}

    void addResult(int messageNum){
        total_ += messageNum;
        if(messageNum == 0){
            ++resList_[0];
        }else if(messageNum < 10){
            ++resList_[1];
        }else if(messageNum < 100){
            ++resList_[2];
        }else if(messageNum < 1000){
            ++resList_[3];
        }else if(messageNum < 10000){
            ++resList_[4];
        }else if(messageNum < 100000){
            ++resList_[5];
        }else if(messageNum < 1000000){
            ++resList_[6];
        }else if(messageNum < 10000000){
            ++resList_[7];
        }else{
            ++resList_[8];
        }
    }

    void printResult(){
        cout << endl;
        cout << "RESULT INFORMATION: " << endl;
        cout << "TOTAL MATCH: " << total_ << endl;
        cout << "0  result: " << resList_[0] << endl;
        cout << "1   ~ 10 : " << resList_[1] << endl;
        cout << "10  ~ 100: " << resList_[2] << endl;
        cout << "100 ~ 1e3: " << resList_[3] << endl;
        cout << "1e3 ~ 1e4: " << resList_[4] << endl;
        cout << "1e4 ~ 1e5: " << resList_[5] << endl;
        cout << "1e5 ~ 1e6: " << resList_[6] << endl;
        cout << "1e6 ~ 1e7: " << resList_[7] << endl;
        cout << "   MORE  : " << resList_[8] << endl;
        cout << endl;
    }

    void resetResult(){
        total_ = 0;
        resList_.assign(resList_.size(), 0);
    }
};

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

class RBVH {
public:
    vector<CRQuery*> queries_;
    queue<int> freeQueryID_;
    size_t regCounter_ = 0;
    Result result_;

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

    ~RBVH() {
        for(auto queryPtr: queries_)
            delete queryPtr;
        delete root_;
    }

    int createQuery(CRQuery* query) {
        int queryID;
        if(freeQueryID_.empty()){
            queryID = queries_.size();
            query->queryID_ = queryID;
            auto* newQuery = new CRQuery(*query);
            queries_.push_back(newQuery);
        }else{
            queryID = freeQueryID_.front();
            freeQueryID_.pop();
            query->queryID_ = queryID;
            queries_[queryID]->copyQuery(*query);
        }
        return queryID;
    }

    void registerCRQuery(CRQuery* query);
    void cancelCRQuery(CRQuery* query);
    void publishPoint(Point* point);
    // Check internal consistency of the index.
    // Must verify that:
    // 1) regCounter_ equals the total number of stored registrations
    // 2) Sum of queries_[i].regiCounter_ equals regCounter_
    // 3) No deleted query appears in index storage
    // Returns true if consistent, false otherwise.
    bool checkConsistency() const;

    Result totalMatch() {
        for(auto& query: queries_){
            if(query->isDelete_) continue;
            result_.addResult(query->mesaCounter_);
        }
        return result_;
    };
    void resetResult(){
        for(auto& query: queries_)
            query->mesaCounter_ = 0;
        result_.resetResult();
    }
    void flush();
    void condenseTree(double curTime);
    void updateSTList(const Point* p, double curTime);
    float getFlushInterval();

    // Only meaningful for R-BVH incremental construction, do not do any matching operation
    // Operations "publishFull" and "publishPart" are denied
    bool incrementalConstruction(Point* point);
    // Only meaningful for R-BVH publishing synchronized query deletion, do not change R-BVH structure
    // Operation "splitNode" is denied
    void publishSynchronizedDelete(Point* point);

    void splitNode(RBVHNode *node);
    void publishFull(RBVHNode *node);
    void publishPart(RBVHNode *node, Point* p);
    void rollBack2Base(RBVHNode *node, int height);
    bool cutHair(RBVHNode *node, int height);
    void cutHead(RBVHNode *node, int height);
    void rebuild();
    bool isDrift(double curTime);
};

#endif //R_BVH_TMP_R_BVH_H
