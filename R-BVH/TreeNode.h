#include <bits/stdc++.h>
#include "Parser.h"
#ifndef TREENODE_H
#define TREENODE_H

class TreeNode{
	public:
		int height;
        int buffer;
        time_t last_upt;
        double start[3];
        double end[3];
        vector<Subscriber*> subscribers;
		TreeNode** children;
		 
	public:
		static int data_max_size;
		
		TreeNode(int subcode, TreeNode *parent){
            this->height = parent->height+1;
            this->buffer = 0;
            this->last_upt = time(nullptr);

            double mid[3];

            for(int idx = 0; idx < 3; ++idx){
                this->start[idx] = parent->start[idx];
                this->end[idx] = parent->end[idx];
                mid[idx] = (this->start[idx] + this->end[idx])/2;
            }

            for(int d = 0; d < 3; ++d){
                if((subcode & 1) == 1){
                    this->start[2-d] = mid[2-d];
                }else{
                    this->end[2-d] = mid[2-d];
                }
                subcode >>= 1;
            }
            this->children = nullptr;
		}

        TreeNode(Parser *parser){
            this->buffer = 0;
            this->last_upt = time(nullptr);
            this->height = -1;
            for(int idx = 0; idx < 3; ++idx){
                this->start[idx] = parser->origin[idx];
                this->end[idx] = parser->range[idx] + this->start[idx];
            }
            this->children = nullptr;
        }
		
		
		~TreeNode(){
            this->subscribers.clear();
            this->subscribers.shrink_to_fit();
            if(this->children != nullptr){
                for(int i = 0; i < 8; ++i){
                    if(this->children[i] != nullptr){
                        delete this->children[i];
                    }
                }
                delete[] this->children;
            }
		}

        static bool second_filter(Point *p, Subscriber *s){
            for(int idx = 0; idx < 3; ++idx){
                if(p->cord[idx] < s->start[idx] || p->cord[idx] > s->end[idx]){
                    return false;
                }
            }
            return true;
        }

		void split(int idx){
			this->children = new TreeNode*[8];
            for(int i = 0; i < 8; ++i){
                this->children[i] = new TreeNode(i, this);
            }
            int relation, c_relation;
            vector<Subscriber*> tmp;
            TreeNode *child;
            for(auto s : this->subscribers){
                relation = Parser::range_relation(this->start, this->end, s);
                if(relation == 1 || relation == 3){
                    for(int i = 0; i < 8; ++i){
                        child = this->children[i];
                        c_relation = Parser::range_relation(child->start, child->end, s);
                        if(c_relation != 0){
                            child->add_sub(s, idx);
                        }
                    }
                }else{
                    tmp.push_back(s);
                }
            }
            this->subscribers.clear();
            this->subscribers.assign(tmp.begin(), tmp.end());
            this->subscribers.shrink_to_fit();
            tmp.clear();
            tmp.shrink_to_fit();
		}

        void merge(int idx, time_t cur_time){
            int c_num = 8;
            set<Subscriber*> tmp;
            for (int i = 0; i < c_num; ++i) {
                tmp.insert(this->children[i]->subscribers.begin(), this->children[i]->subscribers.end());
                delete this->children[i];
            }
            this->subscribers.insert(this->subscribers.end(), tmp.begin(), tmp.end());
            tmp.clear();
            delete[] this->children;
            this->children = nullptr;
            this->last_upt = cur_time;
        }

		void add_sub(Subscriber *s, int idx) {
			this->subscribers.push_back(s);
		}
		
		void publish(Point *p) {
            int left = 0, right = (int)this->subscribers.size()-1;
            Subscriber *ls, *rs;
            if (this->children == nullptr) {
                while (left < right){
                    while (left < right){
                        ls = this->subscribers[left];
                        if (!ls->is_delete){
                            if (TreeNode::second_filter(p, ls)){ls->recv_mesa();}
                            ++left;
                        } else {break;}
                    }
                    if (left >= right){break;}
                    while (left < right){
                        rs = this->subscribers[right];
                        if(rs->is_delete){--right;} else {
                            if (TreeNode::second_filter(p, rs)){rs->recv_mesa();}
                            break;
                        }
                    }
                    if (left >= right){break;}
                    swap(this->subscribers[left], this->subscribers[right]);
                }
                if(left == this->subscribers.size()-1 && !this->subscribers[left]->is_delete){
                    ls = this->subscribers[left];
                    if (TreeNode::second_filter(p, ls)){ls->recv_mesa();}
                    ++left;}
                this->subscribers.erase(this->subscribers.begin()+left, this->subscribers.end());
            } else {
                if (++this->buffer == TreeNode::data_max_size) {
                    while (left < right){
                        while (left < right){
                            ls = this->subscribers[left];
                            if (!ls->is_delete){
                                ls->recv_mesa(this->buffer);
                                ++left;
                            } else {break;}
                        }
                        if (left >= right){break;}
                        while (left < right){
                            rs = this->subscribers[right];
                            if(rs->is_delete){--right;} else {
                                rs->recv_mesa(this->buffer);
                                break;
                            }
                        }
                        if (left >= right){break;}
                        swap(this->subscribers[left], this->subscribers[right]);
                    }
                    if(left == this->subscribers.size()-1 && !this->subscribers[left]->is_delete){
                        this->subscribers[left]->recv_mesa(this->buffer);
                        ++left;}
                    this->subscribers.erase(this->subscribers.begin()+left, this->subscribers.end());
                    this->buffer = 0;
                }
            }
        }
};

int TreeNode::data_max_size = 5;
#endif
