#include <bits/stdc++.h>
#include "Point.h"
#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

class Subscriber{
	public:
		int id_{};
        bool is_delete_;
        double low_[3]{};
        double top_[3]{};
		int message_counter_;
		
	public:
		Subscriber(int id, const vector<double>& start, const vector<double>& end){
			this->id_ = id;
            for(int idx = 0; idx < 3; ++idx){
                this->low_[idx] = start[idx];
                this->top_[idx] = end[idx];
            }
			this->message_counter_ = 0;
            this->is_delete_ = false;
		}
		Subscriber(const Subscriber &s){
			this->id_ = s.id_;
            for(int idx = 0; idx < 3; ++idx){
                this->low_[idx] = s.low_[idx];
                this->top_[idx] = s.top_[idx];
            }
			this->message_counter_ = s.message_counter_;
            this->is_delete_ = s.is_delete_;
		}
		Subscriber(){
			this->message_counter_ = 0;
            this->is_delete_ = false;
		}
		~Subscriber()= default;

        void Set(int id, const vector<double>& low, const vector<double>& top){
            this->id_ = id;
            for(int idx = 0; idx < 3; ++idx){
                this->low_[idx] = low[idx];
                this->top_[idx] = top[idx];
            }
            this->is_delete_ = false;
            this->message_counter_ = 0;
        }
		
		void recv_mesa(int mesa_num = 1){
			this->message_counter_ += mesa_num;
		}
};

#endif
