#include "Geometry.h"

void Point::setCord(const vector<double>& cord){
    assert(cord.size() == DIM);
    cord_.assign(cord.begin(), cord.end());
}

void Point::setCord(Point *p){
    assert(p->cord_.size() == DIM);
    cord_.assign(p->cord_.begin(), p->cord_.end());
    mortonCode_ = p->mortonCode_;
}

void Point::getMortonCode(const vector<double>& LOW, vector<double>& TOP){
    assert(LOW.size() == DIM && TOP.size() == DIM);
    int axis[DIM];
    for(int dim = 0; dim < DIM; dim++){
        axis[dim] = static_cast<int>((cord_[dim] - LOW[dim]) / ((TOP[dim] - LOW[dim]) / (1<<10)));
    }
    mortonCode_ = "";
    for(int height = 9; height >= 0; --height){
        for(int axi : axis){
            mortonCode_ += to_string((axi >> height) & 1);
        }
    }
}

int Point::getSubCode(int height) const{
    string subCode = mortonCode_.substr(DIM * height, DIM);
    return stoi(subCode, nullptr, 2);
}


void Rectangle::setBound(const Rectangle& rect){
    assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
    low_.assign(rect.low_.begin(), rect.low_.end());
    top_.assign(rect.top_.begin(), rect.top_.end());
}

void Rectangle::setBound(const vector<double>& low, const vector<double>& top){
    assert(low.size() == DIM && top.size() == DIM);
    low_.assign(low.begin(), low.end());
    top_.assign(top.begin(), top.end());
}

double Rectangle::getPerimeter()const {
    double perimeter = 0.0;
    for(int dim = 0; dim < DIM; ++dim)
        perimeter += (top_[dim] - low_[dim]);
    return perimeter;
}

double Rectangle::getVolume() const {
    if(!isValid()) return 0.0;
    double volume = 1.0;
    for(int dim = 0; dim < DIM; ++dim)
        volume *= (top_[dim] - low_[dim]);
    assert(volume > 0);
    return volume;
}

double Rectangle::getDistance(const Rectangle& rect) const {
    assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
    double distance = 0.0;
    for(int dim = 0; dim < DIM; ++dim){
        double c1 = (low_[dim] + top_[dim]) * 0.5;
        double c2 = (rect.low_[dim] + rect.top_[dim]) * 0.5;
        distance += (c1 - c2) * (c1 - c2);
    }
    return std::sqrt(distance);
}

bool Rectangle::isValid() const {
    for(int dim = 0; dim < DIM; ++dim)
        if(low_[dim] >= top_[dim]) return false;
    return true;
}

bool Rectangle::isEqual(const Rectangle& rect) const {
    assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
    double epsilon = 1e-12;
    for(int dim = 0; dim < DIM; ++dim){
        if(abs(low_[dim] - rect.low_[dim]) > epsilon
           || abs(top_[dim] - rect.top_[dim]) > epsilon)
            return false;
    }
    return true;
}

bool Rectangle::isOverlap(const Rectangle& rect) const {
    for(int dim = 0; dim < DIM; ++dim)
        if(low_[dim] >= rect.top_[dim] || top_[dim] <= rect.low_[dim])
            return false;
    return true;
}

bool Rectangle::isContain(const Rectangle& rect) const {
    for(int dim = 0; dim < DIM; ++dim){
        if(low_[dim] <= rect.low_[dim] && top_[dim] >= rect.top_[dim])
            continue;
        else return false;
    }
    return true;
}

bool Rectangle::isContain(const Point &p) const {
    for(int dim = 0; dim < DIM; ++dim)
        if(low_[dim] > p.cord_[dim] || top_[dim] < p.cord_[dim])
            return false;
    return true;
}

void Rectangle::merge(const Rectangle& rect){
    for(int dim = 0; dim < DIM; ++dim){
        low_[dim] = min(low_[dim], rect.low_[dim]);
        top_[dim] = max(top_[dim], rect.top_[dim]);
    }
}

Rectangle Rectangle::getMerge(const Rectangle& rect) const{
    Rectangle rectangle(low_, top_);
    rectangle.merge(rect);
    return rectangle;
}

Rectangle Rectangle::getOverlap(const Rectangle& rect) const{
    Rectangle rectangle;
    for(int dim = 0; dim < DIM; ++dim){
        rectangle.low_[dim] = max(low_[dim], rect.low_[dim]);
        rectangle.top_[dim] = min(top_[dim], rect.top_[dim]);
    }
    return rectangle;
}

Relation Rectangle::getRelation(const Rectangle &rect) const {
    assert(rect.low_.size() == DIM && rect.top_.size() == DIM);
    int Cover = 0, Covered = 0;
    for(int dim = 0; dim < DIM; ++dim){
        if(low_[dim] > rect.top_[dim] || top_[dim] < rect.low_[dim])
            return Disjoint;
        else if(low_[dim] < rect.low_[dim] && top_[dim] > rect.top_[dim])
            ++Cover;
        else if(low_[dim] >= rect.low_[dim] && top_[dim] <= rect.top_[dim])
            ++Covered;
    }
    if(Cover == DIM) //node fully covers the query
        return Contain;
    else if(Covered == DIM) // node is fully covered by query
        return Contained;
    else // node partly intersects query
        return Joint;
}
