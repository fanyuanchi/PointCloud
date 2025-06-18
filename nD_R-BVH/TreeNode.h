#include <bits/stdc++.h>
#include "Parser.h"
using namespace std; 
#ifndef TREENODE_H
#define TREENODE_H

class TreeNode{
	public:
		int height;
        double start[3];
        double end[3];
        vector<Subscriber*> subscribers;
		TreeNode** children;
		 
	public:
		
		TreeNode(int subcode, TreeNode *parent){
			this->height = parent->height+1;

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
            this->height = -1;
            for(int idx = 0; idx < 3; ++idx){
                this->start[idx] = parser->origin[idx];
                this->end[idx] = parser->range[idx] + this->start[idx];
            }
            this->children = nullptr;
        }
		
		
		~TreeNode(){
			for(int i = 0; i < 8; ++i){
				delete this->children[i];
			}
			delete[] this->children;
		}

		bool full_filter(Point *p, Subscriber *s){
            for(int idx = 0; idx < DIMS; ++idx){
                if(p->coordinate[idx] < s->start[idx] || p->coordinate[idx] > s->end[idx]){
                    return false;
                }
            }
            return true;
		}

        bool part_filter(Point *p, Subscriber *s){
            for(int idx = 3; idx < DIMS; ++idx){
                if(p->coordinate[idx] < s->start[idx] || p->coordinate[idx] > s->end[idx]){
                    return false;
                }
            }
            return true;
        }

		void split(Parser *parser, int idx){
			this->children = new TreeNode*[8];
			for(int i = 0; i < 8; ++i){
				this->children[i] = new TreeNode(i, this);
			}
            int relation, c_relation;
            vector<Subscriber*> tmp;
            TreeNode *child;
            for(auto s : this->subscribers){
                relation = parser->range_relation(this->start, this->end, s);
                if(relation == 1 || relation == 3){
                    for(int i = 0; i < 8; ++i){
                        child = this->children[i];
                        c_relation = parser->range_relation(child->start, child->end, s);
                        if(c_relation != 0){
                            child->subscribers.push_back(s);;
                        }
                    }
                }else{
                    tmp.push_back(s);
                }
            }
            this->subscribers.clear();
            this->subscribers.assign(tmp.begin(), tmp.end());
            tmp.clear();
		}
		
		void publish(Point *p) {
            if (this->children == nullptr) {
                for(auto s: this->subscribers){
                    if(this->full_filter(p, s)){
                        s->recv_mesa();
                    }
                }
            } else {
                for(auto s: this->subscribers){
                    if(this->part_filter(p, s)){
                        s->recv_mesa();
                    }
                }
            }
        }
};
#endif
