#include <bits/stdc++.h>
using namespace std;
#ifndef POINT_H
#define POINT_H
class Point{
	public:
		double x, y, z;
		double intensity, bit_fields;
		unsigned long long morton_code;	//Äª¶ÙÂë
	
	public:
		Point(vector<double> info){
			this->x = info.at(0); this->y = info.at(1); this->z = info.at(2);
			if(info.size() == 3){
				this->intensity = 0; this->bit_fields = 0;
			}else{
				this->intensity = info.at(3); this->bit_fields = info.at(4);
			}
		}
		Point(const Point &p){
			this->x = p.x; this->y = p.y; this->z = p.z;
			this->intensity = p.intensity; this->bit_fields = p.bit_fields;
            this->morton_code = p.morton_code;
		}
		Point(double x, double y, double z){
			this->x = x; this->y = y; this->z = z;
			this->intensity = 0; this->bit_fields = 0;
            this->morton_code = 0;
		}
		Point()= default;
		
		~Point()= default;
		
		void p_assign(vector<double> &info){
			this->x = info.at(0); this->y = info.at(1); this->z = info.at(2);
			if(info.size() == 3){
				this->intensity = 0; this->bit_fields = 0;
			} else {
				this->intensity = info.at(3); this->bit_fields = info.at(4);
			}
		}
		
		void p_assign(Point *p){
			this->x = p->x; this->y = p->y; this->z = p->z;
			this->intensity = p->intensity; this->bit_fields = p->bit_fields;
			this->morton_code = p->morton_code;
		}
		
		void p_assign(double x, double y, double z){
			this->x = x; this->y = y; this->z = z;
		}
		
		string p_string() const{
			string info;
			info += "code = "; info += to_string(this->morton_code); 
			info += ", x = "; info += to_string(this->x); 
			info += ", y = "; info += to_string(this->y); 
			info += ", z = "; info += to_string(this->z);
			return info;
		}	
};

#endif
