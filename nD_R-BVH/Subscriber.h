#include <bits/stdc++.h>
#include "Point.h"
using namespace std;
#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

class Subscriber{
	public:
		int id{};
		double start[DIMS];
        double end[DIMS];
		int mesa_counter;
        bool is_delete;
		
	public:
		Subscriber(int id, const vector<double> &start,  const vector<double> &end){
			this->id = id;
            for(int idx = 0; idx < DIMS; ++idx){
                this->start[idx] = start[idx];
                this->end[idx] = end[idx];
            }
			this->mesa_counter = 0;
            this->is_delete = false;
		}

		Subscriber(){
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		~Subscriber()= default;
		
		void s_assign(int id, const vector<double> start, const vector<double> end){
			this->id = id;
            for(int idx = 0; idx < DIMS; ++idx){
                this->start[idx] = start[idx];
                this->end[idx] = end[idx];
            }
            this->is_delete = false;
            this->mesa_counter = 0;
		}
		
		void recv_mesa(int mesa_num = 1){
			this->mesa_counter += mesa_num;
		}
};

#endif
