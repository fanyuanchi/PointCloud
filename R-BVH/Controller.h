#include <bits/stdc++.h>
#include "TreeNode.h"
#ifndef CONTROLLER_H
#define CONTROLLER_H

#define PARA 32

class Controller{
	public:
		TreeNode **root_;
		Parser *parser_;
        Subscriber *subscribers_;
        Point *points_;
        int point_size_;
        bool is_complete_[PARA]{};
        int subscriber_index_;
		mutex mutex_;
        mutex *daemon_mutex_;

        static int leaf_capacity;

	public:
		Controller(Parser *parser, Subscriber* subscribers, Point *points, int point_size){
			this->parser_ = parser;
            this->subscribers_ = subscribers;
            this->points_ = points;
            this->point_size_ = point_size;
			this->root_ = new TreeNode*[PARA];
            for(int i = 0; i < PARA; ++i){
                this->root_[i] = new TreeNode(parser);
                this->root_[i]->split_tree_node();
                this->is_complete_[i] = false;
            }
            this->subscriber_index_ = 0;
            this->daemon_mutex_ = new mutex[PARA];
        }

        void dfs_flush_merge(TreeNode *current_node, time_t cur_time) const {
            if(current_node->buffer_ != 0){
                int left_index = 0, right_index = (int)current_node->full_covered_subscribers_.size()-1;
                Subscriber *left_subscriber, *right_subscriber;
                while (left_index < right_index){
                    while (left_index < right_index){
                        left_subscriber = current_node->full_covered_subscribers_[left_index];
                        if (!left_subscriber->is_delete_){
                            left_subscriber->recv_mesa(current_node->buffer_);
                            ++left_index;
                        } else break;
                    }
                    if (left_index >= right_index) break;
                    while (left_index < right_index){
                        right_subscriber = current_node->full_covered_subscribers_[right_index];
                        if(right_subscriber->is_delete_) --right_index;
                        else {
                            right_subscriber->recv_mesa(current_node->buffer_);
                            break;
                        }
                    }
                    if (left_index >= right_index) break;
                    swap(current_node->full_covered_subscribers_[left_index],
                         current_node->full_covered_subscribers_[right_index]);
                    ++left_index; --right_index;
                }
                if(left_index < current_node->full_covered_subscribers_.size()
                && !current_node->full_covered_subscribers_[left_index]->is_delete_){
                    current_node->full_covered_subscribers_[left_index]->recv_mesa(current_node->buffer_);
                    ++left_index;
                }
                current_node->full_covered_subscribers_.erase(current_node->full_covered_subscribers_.begin()+left_index,
                                                              current_node->full_covered_subscribers_.end());
                current_node->buffer_ = 0;
            }
            if(current_node->children_ == nullptr) return;
            for(int i = 0; i < 8; ++i){
                dfs_flush_merge(current_node->children_[i], cur_time);
            }
            if((cur_time - current_node->last_update_time_) > 3)
                current_node->merge_tree_node(cur_time);
            else
                current_node->last_update_time_ = cur_time;
        }

        void daemon(int interval){
            int flag;
            while(true){
                flag = 0;
                time_t cur_time = time(nullptr);
                for(int i = 0; i < PARA; ++i){
                    if(!this->is_complete_[i]){
                        lock_guard<mutex> lock(this->daemon_mutex_[i]);
                        this->dfs_flush_merge(this->root_[i], cur_time);
                        flag = 1;
                    }
                }
                if(flag == 0) break;
                else this_thread::sleep_for(chrono::seconds(interval));
            }
        }

        void publisher(int thread_index){
            Point* p;
            TreeNode *current_node;
            int sub_code;

            time_t current_time, last_time, start, end;
            start = time(nullptr);
            last_time = start;

            for (int point_index = 0; point_index < this->point_size_; ++point_index){
                current_time = time(nullptr);
                p = &this->points_[point_index];
                current_node = this->root_[thread_index];
                {
                    lock_guard<mutex> lock(this->daemon_mutex_[thread_index]);
                    while (true) {
                        current_node->last_update_time_ = current_time;
                        if(current_node->children_ == nullptr){
                            if(current_node->height_ < this->parser_->h_max_-1
                            && current_node->part_covered_subscribers_.size() >= Controller::leaf_capacity){
                                current_node->split_tree_node();
                            }else{
                                current_node->publish(p);
                                break;
                            }
                        }
                        if(current_node->children_ != nullptr){
                            current_node->publish(p);
                            sub_code = this->parser_->get_subcode(p, current_node->height_);
                            current_node = current_node->children_[sub_code];
                        }
                    }
                }
//                if((point_index+1) % 100000 == 0){
//                    current_time = time(nullptr);
//                    cout << current_time - last_time << " / " << current_time - start << " seconds passed, Thread "
//                         << thread_index << " has published 100000" << " / " << (point_index+1) << " data." << endl << endl;
//                    last_time = current_time;
//                }
            }

            end = time(nullptr);
            {
                lock_guard<mutex> lock(this->daemon_mutex_[thread_index]);
                this->dfs_flush_merge(this->root_[thread_index], end);
                this->is_complete_[thread_index] = true;
            }
            cout << endl << endl
                 << "Thread " << thread_index << " completes mission, takes " << end - start << " seconds."
                 << endl << endl << endl;
        }

        void multi_reg(int subs_size, int thread_index){
            while(true){
                int current_idx;
                {
                    lock_guard<mutex> lock(this->mutex_);
                    current_idx = this->subscriber_index_++;
                }
                if(current_idx >= subs_size) break;
                else {
                    {
                        lock_guard<mutex> lock(this->daemon_mutex_[thread_index]);
                        this->registration(&this->subscribers_[current_idx], thread_index);
                    }
                }
            }
        }

		void registration(Subscriber *subscriber, int idx) const{
			queue<TreeNode*> que;
			TreeNode *current_node;
			enum Relation relation;

			que.push(this->root_[idx]);
			while(!que.empty()){
				current_node = que.front();
				que.pop();
				relation = Parser::range_relation(current_node->low_, current_node->top_, subscriber);
				if(relation == DisJoint) continue;
                if(relation == Contained){
                    current_node->full_covered_subscribers_.push_back(subscriber);
                }else{
                    if(current_node->children_ == nullptr)
                        current_node->part_covered_subscribers_.push_back(subscriber);
                    else
                        for(int i = 0; i < 8; ++i)
                            que.push(current_node->children_[i]);
                }
			}
		}
};

int Controller::leaf_capacity = 64;
#endif
