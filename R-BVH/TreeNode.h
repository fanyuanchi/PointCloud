#include <bits/stdc++.h>
#include "Parser.h"
#ifndef TREENODE_H
#define TREENODE_H

class TreeNode{
	public:
		int height_ = 0;
        int buffer_ = -1;
        double low_[3]{};
        double top_[3]{};
        time_t last_update_time_;
        vector<Subscriber*> full_covered_subscribers_;
        vector<Subscriber*> part_covered_subscribers_;
		TreeNode** children_;
		 
	public:
		static int buffer_capacity;
		
		TreeNode(int sub_code, TreeNode *parent){
            this->height_ = parent->height_+1;
            this->buffer_ = 0;
            this->last_update_time_ = time(nullptr);

            double mid[3];

            for(int idx = 0; idx < 3; ++idx){
                this->low_[idx] = parent->low_[idx];
                this->top_[idx] = parent->top_[idx];
                mid[idx] = (this->low_[idx] + this->top_[idx])/2;
            }

            for(int d = 0; d < 3; ++d){
                if((sub_code & 1) == 1){
                    this->low_[2-d] = mid[2-d];
                }else{
                    this->top_[2-d] = mid[2-d];
                }
                sub_code >>= 1;
            }
            this->children_ = nullptr;
		}

        TreeNode(Parser *parser){
            this->buffer_ = 0;
            this->height_ = -1;
            this->last_update_time_ = time(nullptr);
            for(int idx = 0; idx < 3; ++idx){
                this->low_[idx] = parser->low_[idx];
                this->top_[idx] = parser->range_[idx] + this->low_[idx];
            }
            this->children_ = nullptr;
        }
		
		
		~TreeNode(){
            this->full_covered_subscribers_.clear();
            this->part_covered_subscribers_.clear();
            if(this->children_ != nullptr){
                for(int i = 0; i < 8; ++i){
                    if(this->children_[i] != nullptr){
                        delete this->children_[i];
                    }
                }
                delete[] this->children_;
            }
		}

        static bool second_filter(Point *p, Subscriber *s){
            for(int idx = 0; idx < 3; ++idx){
                if(p->cord_[idx] < s->low_[idx] || p->cord_[idx] > s->top_[idx]){
                    return false;
                }
            }
            return true;
        }

		void split_tree_node(){
			this->children_ = new TreeNode*[8];
            for(int i = 0; i < 8; ++i){
                this->children_[i] = new TreeNode(i, this);
            }
            enum Relation relation;
            TreeNode *child;
            for(auto subscriber : this->part_covered_subscribers_){
                for(int i = 0; i < 8; ++i){
                    child = this->children_[i];
                    relation = Parser::range_relation(child->low_, child->top_, subscriber);
                    if(relation == Contained){
                        child->full_covered_subscribers_.push_back(subscriber);
                    }else if(relation != DisJoint){
                        child->part_covered_subscribers_.push_back(subscriber);
                    }
                }
            }
            this->part_covered_subscribers_.clear();
            this->part_covered_subscribers_.resize(0);
		}

        void merge_tree_node(time_t current_time){
            int child_num = 8;
            set<Subscriber*> tmp;
            for (int i = 0; i < child_num; ++i) {
                tmp.insert(this->children_[i]->part_covered_subscribers_.begin(),
                           this->children_[i]->part_covered_subscribers_.end());
                delete this->children_[i];
            }
            this->part_covered_subscribers_.assign(tmp.begin(), tmp.end());
            tmp.clear();
            delete[] this->children_;
            this->children_ = nullptr;
            this->last_update_time_ = current_time;
        }

        void publish(Point *p) {
            int left_index = 0, right_index = (int)this->full_covered_subscribers_.size()-1;
            Subscriber *left_subscriber, *right_subscriber;
            if (++this->buffer_ == TreeNode::buffer_capacity) {
                while (left_index < right_index){
                    while (left_index < right_index){
                        left_subscriber = this->full_covered_subscribers_[left_index];
                        if (!left_subscriber->is_delete_){
                            left_subscriber->recv_mesa(this->buffer_);
                            ++left_index;
                        } else break;
                    }
                    if (left_index >= right_index) break;
                    while (left_index < right_index){
                        right_subscriber = this->full_covered_subscribers_[right_index];
                        if(right_subscriber->is_delete_) --right_index;
                        else {
                            right_subscriber->recv_mesa(this->buffer_);
                            break;
                        }
                    }
                    if (left_index >= right_index) break;
                    swap(this->full_covered_subscribers_[left_index],
                         this->full_covered_subscribers_[right_index]);
                    ++left_index;
                    --right_index;
                }
                if(left_index == this->full_covered_subscribers_.size()-1
                && !this->full_covered_subscribers_[left_index]->is_delete_){
                    this->full_covered_subscribers_[left_index]->recv_mesa(this->buffer_);
                    ++left_index;
                }
                this->full_covered_subscribers_.erase(this->full_covered_subscribers_.begin()+left_index,
                                                      this->full_covered_subscribers_.end());
                this->buffer_ = 0;
            }
            if (this->children_ == nullptr) {
                left_index = 0; right_index = (int)this->part_covered_subscribers_.size()-1;
                while (left_index < right_index){
                    while (left_index < right_index){
                        left_subscriber = this->part_covered_subscribers_[left_index];
                        if (!left_subscriber->is_delete_){
                            if (TreeNode::second_filter(p, left_subscriber))
                                left_subscriber->recv_mesa();
                            ++left_index;
                        } else break;
                    }
                    if (left_index >= right_index) break;
                    while (left_index < right_index){
                        right_subscriber = this->part_covered_subscribers_[right_index];
                        if(right_subscriber->is_delete_) --right_index;
                        else {
                            if (TreeNode::second_filter(p, right_subscriber))
                                right_subscriber->recv_mesa();
                            break;
                        }
                    }
                    if (left_index >= right_index) break;
                    swap(this->part_covered_subscribers_[left_index],
                         this->part_covered_subscribers_[right_index]);
                    ++left_index; --right_index;
                }
                if(left_index == this->part_covered_subscribers_.size()-1
                && !this->part_covered_subscribers_[left_index]->is_delete_){
                    left_subscriber = this->part_covered_subscribers_[left_index];
                    if (TreeNode::second_filter(p, left_subscriber)){left_subscriber->recv_mesa();}
                    ++left_index;
                }
                this->part_covered_subscribers_.erase(this->part_covered_subscribers_.begin()+left_index,
                                                      this->part_covered_subscribers_.end());
            }
        }
};

int TreeNode::buffer_capacity = 1000;
#endif
