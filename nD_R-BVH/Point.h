#include <bits/stdc++.h>
using namespace std;
#ifndef POINT_H
#define POINT_H
class Point{
	public:
		vector<double> coordinate;
		unsigned long long morton_code;	//Äª¶ÙÂë
	
	public:
		Point(const vector<double> &info){
			this->coordinate.assign(info.begin(), info.end());
		}
		Point(const Point &p){
			this->coordinate.assign(p.coordinate.begin(), p.coordinate.end());
            this->morton_code = p.morton_code;
		}

		Point()= default;
		
		~Point()= default;
		
		void p_assign(const vector<double> &info){
            this->coordinate.assign(info.begin(), info.end());
		}

        void p_assign(const vector<double> &info, int dimension){
            this->coordinate.assign(info.begin(), info.begin()+dimension);
        }
};

#endif
