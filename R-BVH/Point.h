#include <bits/stdc++.h>
using namespace std;
#ifndef POINT_H
#define POINT_H
class Point{
	public:
		double cord_[3]{};
		unsigned long long morton_code_{};
	
	public:
		Point(const Point &p){
            for(int idx = 0; idx < 3; ++idx){
                this->cord_[idx] = p.cord_[idx];
            }
            this->morton_code_ = p.morton_code_;
		}
		Point()= default;
		
		~Point()= default;
		
		void Set(vector<double> &info){
            for(int idx = 0; idx < 3; ++idx){
                this->cord_[idx] = info[idx];
            }
		}
		
		void Set(Point *p){
            for(int idx = 0; idx < 3; ++idx){
                this->cord_[idx] = p->cord_[idx];
            }
			this->morton_code_ = p->morton_code_;
		}
		
		void Set(double x, double y, double z){
            this->cord_[0] = x;
            this->cord_[1] = y;
            this->cord_[2] = z;
		}
};

#endif
