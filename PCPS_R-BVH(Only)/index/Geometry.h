#ifndef R_BVH_TMP_GEOMETRY_H
#define R_BVH_TMP_GEOMETRY_H

#include <bits/stdc++.h>
using namespace std;

extern int DIM;

class Point{
public:
    vector<double> cord_{};
    string mortonCode_;
    explicit Point(const Point *p){
        assert(p->cord_.size() == DIM);
        cord_.assign(p->cord_.begin(), p->cord_.end());
        mortonCode_ = p->mortonCode_;
    }
    Point(): cord_(DIM){};

    ~Point()= default;

    void setCord(const vector<double>& cord);
    void setCord(Point *p);

    void getMortonCode(const vector<double>& LOW, vector<double>& TOP);
    int getSubCode(int height) const;
};

enum Relation {Contain, Contained, Joint, Disjoint};

class Rectangle{
public:
    vector<double> low_{};
    vector<double> top_{};

    Rectangle(): low_(DIM), top_(DIM) {};
    Rectangle(const vector<double>& low, const vector<double>& top) {
        assert(low.size() == DIM && top.size() == DIM);
        low_.assign(low.begin(), low.end());
        top_.assign(top.begin(), top.end());
    }
    ~Rectangle()= default;

    virtual void setBound(const Rectangle& rect);
    virtual void setBound(const vector<double>& low, const vector<double>& top);

    double getPerimeter()const;
    double getVolume() const;
    double getDistance(const Rectangle& rect) const;

    bool isValid() const;

    virtual bool isEqual(const Rectangle& rect) const;

    virtual bool isOverlap(const Rectangle& rect) const;

    virtual bool isContain(const Rectangle& rect) const;
    virtual bool isContain(const Point &p) const;

    void merge(const Rectangle& rect);

    Rectangle getMerge(const Rectangle& rect) const;
    Rectangle getOverlap(const Rectangle& rect) const;

    virtual Relation getRelation(const Rectangle &rect) const;
};

class CRQuery: public Rectangle{
public:
    bool isDelete_ = false;
    float MLP_ = 0.1;
    int mesaCounter_ = 0;
    int regiCounter_ = 0;
    int queryID_ = -1;
    int indexID_ = -1;

    CRQuery() = default;
    CRQuery(const CRQuery&) = default;
    CRQuery(CRQuery& query) : Rectangle(query.low_, query.top_) {
        assert(query.low_.size() == DIM && query.top_.size() == DIM);
        queryID_ = query.queryID_;
        indexID_ = query.indexID_;
        MLP_ = query.MLP_;
    }

    void copyQuery(CRQuery& query){
        assert(query.low_.size() == DIM && query.top_.size() == DIM);
        low_.assign(query.low_.begin(), query.low_.end());
        top_.assign(query.top_.begin(), query.top_.end());
        isDelete_ = query.isDelete_;
        MLP_ = query.MLP_;
        mesaCounter_ = query.mesaCounter_;
        regiCounter_ = query.regiCounter_;
        queryID_ = query.queryID_;
        indexID_ = query.indexID_;
    }
    void recvMesa(int message = 1){
        mesaCounter_ += message;
    }
};

#endif //R_BVH_TMP_GEOMETRY_H
