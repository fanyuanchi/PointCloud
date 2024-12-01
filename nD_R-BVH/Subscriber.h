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
		vector<double> start;
        vector<double> end;
		int mesa_counter;
		
	public:
		Subscriber(int id, const vector<double> &start,  const vector<double> &end){
			this->id = id;
			this->start.assign(start.begin(), start.end());
            this->end.assign(end.begin(), end.end());
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		Subscriber(const Subscriber &s){
			this->id = s.id;
			this->start.assign(s.start.begin(), s.end.end());
            this->end.assign(s.end.begin(), s.end.end());
			this->mesa_counter = s.mesa_counter;
            this->is_delete = s.is_delete;
		}
		Subscriber(){
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		~Subscriber()= default;
		
		void s_assign(int id, const vector<double> start, const vector<double> end){
			this->id = id;
            this->start.assign(start.begin(), start.end());
            this->end.assign(end.begin(), end.end());
            this->is_delete = false;
            this->mesa_counter = 0;
		}
		
		pair<vector<double>, vector<double> > get_query_range(){
			return make_pair(this->start, this->end);
		}
		
		void recv_mesa(int mesa_num = 1){	//接收订阅信息
			this->mesa_counter += mesa_num;
		}
};

#endif
