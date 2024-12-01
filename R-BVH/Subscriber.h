#include <bits/stdc++.h>
#include "Point.h"
#include "Statistics.h"
using namespace std;
#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

class Subscriber{
	public:
		int id{};				//用户id，用于标识用户身份
        bool is_delete;
		Point p1{}, p2{};
		int mesa_counter;
		
	public:
		Subscriber(int id, const Point& p1, const Point& p2){
			this->id = id;
			this->p1 = p1; this->p2 = p2;
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		Subscriber(const Subscriber &s){
			this->id = s.id;
			this->p1 = s.p1; this->p2 = s.p2;
			this->mesa_counter = s.mesa_counter;
            this->is_delete = s.is_delete;
		}
		Subscriber(){
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		~Subscriber()= default;
		
		void s_assign(int id, vector<double> &v1, vector<double> &v2){
			this->id = id;
			this->p1.p_assign(v1);
			this->p2.p_assign(v2);
            this->is_delete = false;
		}
		
		void s_assign(int id, double p1_x,  double p1_y,  double p1_z,  double p2_x,  double p2_y,  double p2_z){
			this->id = id;
            this->mesa_counter = 0;
			this->p1.p_assign(p1_x, p1_y, p1_z);
			this->p2.p_assign(p2_x, p2_y, p2_z);
            this->is_delete = false;
		}
		
		pair<Point*, Point*> get_query_range(){
			return make_pair(&this->p1, &this->p2);
		}
		
		string s_string(){
			string info;
			info += "Sub ID: ";
			info += to_string(this->id);
			info += ", QR: P1 = ";
			info += this->p1.p_string();
			info += "; P2 = ";
			info += this->p2.p_string();
			return info;
		} 
		
		void recv_mesa(int mesa_num = 1){	//接收订阅信息
			this->mesa_counter += mesa_num;
		}
};

#endif
