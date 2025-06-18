#include <bits/stdc++.h>
#include "Point.h"
#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

class Subscriber{
	public:
		int id{};
        bool is_delete;
        double start[3];
        double end[3];
		int mesa_counter;
		
	public:
		Subscriber(int id, const vector<double>& start, const vector<double>& end){
			this->id = id;
            for(int idx = 0; idx < 3; ++idx){
                this->start[idx] = start[idx];
                this->end[idx] = end[idx];
            }
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		Subscriber(const Subscriber &s){
			this->id = s.id;
            for(int idx = 0; idx < 3; ++idx){
                this->start[idx] = s.start[idx];
                this->end[idx] = s.end[idx];
            }
			this->mesa_counter = s.mesa_counter;
            this->is_delete = s.is_delete;
		}
		Subscriber(){
			this->mesa_counter = 0;
            this->is_delete = false;
		}
		~Subscriber()= default;

        void Set(int id, const vector<double> start, const vector<double> end){
            this->id = id;
            for(int idx = 0; idx < 3; ++idx){
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
