#include <bits/stdc++.h>
using namespace std;
#ifndef POINT_H
#define POINT_H

#define DIMS 3

class Point{
	public:
        int id{};
		double coordinate[DIMS];
		unsigned long long morton_code;
	
	public:
		Point(const vector<double> &info){
            for(int idx = 0; idx < DIMS; ++idx){
                this->coordinate[idx] = info[idx];
            }
		}
		Point(const Point &p){
            for(int idx = 0; idx < DIMS; ++idx){
                this->coordinate[idx] = p.coordinate[idx];
            }
            this->morton_code = p.morton_code;
		}

		Point()= default;
		
		~Point()= default;
		
		void p_assign(const vector<double> &info, int id){
            this->id = id;
            for(int idx = 0; idx < DIMS; ++idx){
                this->coordinate[idx] = info[idx];
            }
		}

};

#endif
