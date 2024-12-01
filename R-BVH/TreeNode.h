#include <bits/stdc++.h>
#include "Point.h"
#include "Subscriber.h"
#include "Parser.h"
#include "Statistics.h"
using namespace std; 
#ifndef TREENODE_H
#define TREENODE_H

class TreeNode{
	public:
		unsigned long long morton_code;
		
		int height;
        int buffer;
        time_t last_upt;
		Point p1{}, p2{};
        vector<Subscriber*> subscribers;
		TreeNode** children;
		 
	public:
		static int data_max_size;
		
		TreeNode(unsigned long long morton_code, int height, Parser* parser){
			this->morton_code = morton_code;
			this->height = height;
            this->buffer = 0;
            this->last_upt = time(nullptr);
			
			pair<Point*, Point*> range = parser->decode(morton_code, height);	
			this->p1.p_assign(range.first); 
			this->p2.p_assign(range.second);
			delete range.first; 
			delete range.second;
			
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
		
		//二次过滤器，用于精确判断一个点是否在该订阅查询范围 
		/**
		false: 该点云数据不在订阅查询范围内
		true:  ……在订阅查询范围内 
		**/
		static bool second_filter(Point *p, Subscriber *s){
			pair<Point*, Point*> query_range = s->get_query_range();
			Point *p1 = query_range.first;
			Point *p2 = query_range.second;
			if(p->x < p1->x || p->x > p2->x){
				return false;
			}
			if(p->y < p1->y || p->y > p2->y){
				return false;
			}
			if(p->z < p1->z || p->z > p2->z){
				return false;
			}
			return true;
		}
		
		// 节点分裂(仅分裂一次)
		// 仅生成空的子节点，然后将该节点数据复制到对应子节点中 
		void split(Parser *parser, int idx){
			int c_num = 8;
			this->children = new TreeNode*[c_num];
			for(int i = 0; i < c_num; ++i){
				unsigned long long subcode = (this->morton_code<<3) + i;
				this->children[i] = new TreeNode(subcode, this->height+1, parser);
			}
            int relation, c_relation;
            vector<Subscriber*> tmp;
            TreeNode *child;
            for(auto s : this->subscribers){
                relation = Parser::range_relation(&this->p1, &this->p2, s);
                if(relation == 1 || relation == 3){
                    for(int i = 0; i < c_num; ++i){
                        child = this->children[i];
                        c_relation = Parser::range_relation(&child->p1, &child->p2, s);
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
                Statistics::reg_num[idx] -= (int)this->children[i]->subscribers.size();
                tmp.insert(this->children[i]->subscribers.begin(), this->children[i]->subscribers.end());
                delete this->children[i];
            }
            Statistics::reg_num[idx] += (int)tmp.size();
            this->subscribers.insert(this->subscribers.end(), tmp.begin(), tmp.end());
            tmp.clear();
            delete[] this->children;
            this->children = nullptr;
            this->last_upt = cur_time;
        }

		void add_sub(Subscriber *s, int idx) {
			this->subscribers.push_back(s);
			Statistics::reg_num[idx]++;
		}
		
		void publish(Point *p) {
            int left = 0, right = (int)this->subscribers.size()-1;
            Subscriber *ls, *rs;
            if (this->children == nullptr) { //叶节点发布前经二次过滤
                while (left < right){
                    while (left < right){
                        ls = this->subscribers.at(left);
                        if (!ls->is_delete){
                            if (TreeNode::second_filter(p, ls)){ls->recv_mesa();}
                            ++left;
                        } else {break;}
                    }
                    if (left >= right){break;}
                    while (left < right){
                        rs = this->subscribers.at(right);
                        if(rs->is_delete){--right;} else {
                            if (TreeNode::second_filter(p, rs)){rs->recv_mesa();}
                            break;
                        }
                    }
                    if (left >= right){break;}
                    swap(this->subscribers.at(left), this->subscribers.at(right));
                }
                if(left == this->subscribers.size()-1 && !this->subscribers.at(left)->is_delete){
                    ls = this->subscribers.at(left);
                    if (TreeNode::second_filter(p, ls)){ls->recv_mesa();}
                    ++left;}
                this->subscribers.erase(this->subscribers.begin()+left, this->subscribers.end());
            } else { //中间节点先缓存后打包批量发布
                if (++this->buffer == TreeNode::data_max_size) {
                    while (left < right){
                        while (left < right){
                            ls = this->subscribers.at(left);
                            if (!ls->is_delete){
                                ls->recv_mesa(this->buffer);
                                ++left;
                            } else {break;}
                        }
                        if (left >= right){break;}
                        while (left < right){
                            rs = this->subscribers.at(right);
                            if(rs->is_delete){--right;} else {
                                rs->recv_mesa(this->buffer);
                                break;
                            }
                        }
                        if (left >= right){break;}
                        swap(this->subscribers.at(left), this->subscribers.at(right));
                    }
                    if(left == this->subscribers.size()-1 && !this->subscribers.at(left)->is_delete){
                        this->subscribers.at(left)->recv_mesa(this->buffer);
                        ++left;}
                    this->subscribers.erase(this->subscribers.begin()+left, this->subscribers.end());
                    this->buffer = 0;
                }
            }
        }
};

int TreeNode::data_max_size = 5;
#endif
