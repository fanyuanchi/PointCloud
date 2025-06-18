#include <bits/stdc++.h>
using namespace std;
#ifndef POINT_H
#define POINT_H
class Point{
	public:
		double cord[3];
		unsigned long long morton_code;
	
	public:
		Point(const Point &p){
            for(int idx = 0; idx < 3; ++idx){
                this->cord[idx] = p.cord[idx];
            }
            this->morton_code = p.morton_code;
		}
		Point()= default;
		
		~Point()= default;
		
		void Set(vector<double> &info){
            for(int idx = 0; idx < 3; ++idx){
                this->cord[idx] = info[idx];
            }
		}
		
		void Set(Point *p){
            for(int idx = 0; idx < 3; ++idx){
                this->cord[idx] = p->cord[idx];
            }
			this->morton_code = p->morton_code;
		}
		
		void Set(double x, double y, double z){
            this->cord[0] = x;
            this->cord[1] = y;
            this->cord[2] = z;
		}
};

#endif
