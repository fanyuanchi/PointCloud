#ifndef PCPS_REGULARGRID_H
#define PCPS_REGULARGRID_H

#include "../../IIndex.h"

enum RangeType {edgeRange, internalRange};

struct CellRange {
    size_t L;
    size_t R;
    RangeType type;
};

class RegularCell{
public:
    vector<int> partCover_{};
    vector<int> fullCover_{};

    void addPart(int queryID){
        partCover_.push_back(queryID);
    }
    void addFull(int queryID){
        fullCover_.push_back(queryID);
    }
};

class RegularGrid: public IIndex{
public:
    int delNum_ = 0;
    vector<double> low_, top_;
    vector<double> cellLength_;
    int curG_ = 200;
    vector<RegularCell> cells_;

    static int gMin_, gMax_;
    static double ratioForDelete_;

    RegularGrid(const vector<double>& LOW, const vector<double>& TOP): curG_(RegularGrid::gMax_){
        if(LOW.size() != DIM || TOP.size() != DIM){
            cout << "LOW.size = " << LOW.size() << ", TOP.size = " << TOP.size() << endl;
            cout << "Needed DIM = " << DIM;
            throw std::runtime_error("unmatched dimensionality");
        }
        low_.assign(LOW.begin(), LOW.end());
        top_.assign(TOP.begin(), TOP.end());
        cellLength_.resize(DIM);
        for(int dim = 0; dim < DIM; ++dim){
            cellLength_[dim] = (TOP[dim] - LOW[dim]) / curG_;
        }
    }

    ~RegularGrid() override = default;

    void registerCRQuery(CRQuery* query) override;
    void cancelCRQuery(CRQuery* query) override;
    void publishPoint(Point* point) override;
    bool checkConsistency() const override;
    void condenseTree(double curTime) override;

    void insertQuery(CRQuery& query);
    void rebuild();
    void fullScan2Del();
};

#endif //PCPS_REGULARGRID_H
