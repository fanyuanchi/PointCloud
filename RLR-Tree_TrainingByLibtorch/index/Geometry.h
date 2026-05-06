#ifndef RLR_TREETRAIN_GEOMETRY_H
#define RLR_TREETRAIN_GEOMETRY_H

#include <bits/stdc++.h>
#include "../util/UTIL.h"
using namespace std;

extern int DIM;

class Point{
public:
    vector<double> cord_{};
    explicit Point(const Point *p){
        assert(p->cord_.size() == DIM);
        cord_.assign(p->cord_.begin(), p->cord_.end());
    }
    Point(const vector<double>& cord){
        assert(cord.size() == DIM);
        cord_.assign(cord.begin(), cord.end());
    };
    Point(): cord_(DIM){};

    ~Point()= default;

    void setCord(const vector<double>& cord){
        assert(cord.size() == DIM);
        cord_.assign(cord.begin(), cord.end());
    }

    void setCord(Point *p){
        assert(p->cord_.size() == DIM);
        cord_.assign(p->cord_.begin(), p->cord_.end());
    }
};

class Rectangle{
public:
    int rectID_ = -1;
    vector<double> low_{};
    vector<double> top_{};

    Rectangle(): low_(DIM), top_(DIM) {};
    Rectangle(const vector<double>& low, const vector<double>& top) {
        assert(low.size() == DIM && top.size() == DIM);
        low_.assign(low.begin(), low.end());
        top_.assign(top.begin(), top.end());
    }
    Rectangle(const Rectangle& rectangle): low_(DIM), top_(DIM){
        for(int dim = 0; dim < DIM; ++dim){
            low_[dim] = rectangle.low_[dim];
            top_[dim] = rectangle.top_[dim];
        }
        rectID_ = rectangle.rectID_;
    }
    ~Rectangle()= default;

    void setBound(const Rectangle& rect){
        assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
        low_.assign(rect.low_.begin(), rect.low_.end());
        top_.assign(rect.top_.begin(), rect.top_.end());
    }
    void setBound(const vector<double>& low, const vector<double>& top){
        assert(low.size() == DIM && top.size() == DIM);
        low_.assign(low.begin(), low.end());
        top_.assign(top.begin(), top.end());
    }

    [[nodiscard]] Point getCenter() const{
        Point center;
        for(int dim = 0; dim < DIM; ++dim)
            center.cord_[dim] = (low_[dim] + top_[dim]) * 0.5;
        return center;
    }

    [[nodiscard]] double getPerimeter()const{
        double perimeter = 0.0;
        for(int dim = 0; dim < DIM; ++dim)
            perimeter += (top_[dim] - low_[dim]);
        return perimeter;
    }
    [[nodiscard]] double getVolume() const{
        if(!isValid()) return 0.0;
        double volume = 1.0;
        for(int dim = 0; dim < DIM; ++dim)
            volume *= (top_[dim] - low_[dim]);
        assert(volume > 0);
        return volume;
    }

    [[nodiscard]] bool isValid() const{
        for(int dim = 0; dim < DIM; ++dim)
            if(low_[dim] >= top_[dim]) return false;
        return true;
    }

    [[nodiscard]] bool isEqual(const Rectangle& rect) const{
        assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
        double epsilon = 1e-12;
        for(int dim = 0; dim < DIM; ++dim){
            if(abs(low_[dim] - rect.low_[dim]) > epsilon
               || abs(top_[dim] - rect.top_[dim]) > epsilon)
                return false;
        }
        return true;
    }
    [[nodiscard]] bool isOverlap(const Rectangle& rect) const{
        for(int dim = 0; dim < DIM; ++dim)
            if(low_[dim] >= rect.top_[dim] || top_[dim] <= rect.low_[dim])
                return false;
        return true;
    }

    [[nodiscard]] bool isContain(const Rectangle& rect) const{
        for(int dim = 0; dim < DIM; ++dim){
            if(low_[dim] <= rect.low_[dim] && top_[dim] >= rect.top_[dim])
                continue;
            else return false;
        }
        return true;
    }
    [[nodiscard]] bool isContain(const Point &p) const{
        for(int dim = 0; dim < DIM; ++dim)
            if(low_[dim] > p.cord_[dim] || top_[dim] < p.cord_[dim])
                return false;
        return true;
    }

    void merge(const Rectangle& rect){
        for(int dim = 0; dim < DIM; ++dim){
            low_[dim] = min(low_[dim], rect.low_[dim]);
            top_[dim] = max(top_[dim], rect.top_[dim]);
        }
    }

    [[nodiscard]] Rectangle getMerge(const Rectangle& rect) const{
        Rectangle rectangle(low_, top_);
        rectangle.merge(rect);
        return rectangle;
    }
    [[nodiscard]] Rectangle getOverlap(const Rectangle& rect) const{
        Rectangle rectangle;
        for(int dim = 0; dim < DIM; ++dim){
            rectangle.low_[dim] = max(low_[dim], rect.low_[dim]);
            rectangle.top_[dim] = min(top_[dim], rect.top_[dim]);
        }
        return rectangle;
    }

};


#endif //RLR_TREETRAIN_GEOMETRY_H
