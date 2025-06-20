#include <bits/stdc++.h>
#include "TreeNode.h"
#include "Point.h"
#include "Subscriber.h"
#include "Parser.h"
using namespace std;
#ifndef CONTROLLER_H
#define CONTROLLER_H

#define PARA 32

class Controller{
	public:
		TreeNode **root;
		Parser *parser;
        Subscriber *subscribers;
        int subs_size;
        Point *data;
        int data_size;

        int sub_idx;
		mutex mtx;

        static int leaf_th;

	public:
		Controller(Parser *parser, Subscriber* subscribers, int subs_size, Point *data, int data_size){
			this->parser = parser;
            this->subscribers = subscribers;
            this->data = data;
            this->subs_size = subs_size;
            this->data_size = data_size;
			this->root = new TreeNode*[PARA];
            for(int i = 0; i < PARA; ++i){
                this->root[i] = new TreeNode(parser);
                this->root[i]->split(this->parser, i);
            }
            this->sub_idx = 0;
            Controller::leaf_th = 128;
		}

        void publisher(int idx) const{
            Point* p;
            TreeNode *cur;
            int subcode;

            time_t cur_time, last_time, start, end;
            start = time(nullptr);
            last_time = start;

            for (int data_it = 0; data_it < this->data_size; data_it++){
                p = &this->data[data_it];
                cur = this->root[idx];
                while (true) {
                    if(cur->children == nullptr){
                        if(cur->height < this->parser->r_max-1 && cur->subscribers.size() >= Controller::leaf_th){
                            cur->split(this->parser, idx);
                        }else{
                            cur->publish(p);
                            break;
                        }
                    }
                    if(cur->children != nullptr){
                        cur->publish(p);
                        subcode = this->parser->get_subcode(p, cur->height);
                        cur = cur->children[subcode];
                    }
                }

                if(data_it > 0 && data_it % 100000 == 0){
                    cur_time = time(nullptr);
                    cout << cur_time - last_time << " / " << cur_time - start << " seconds passed, Thread "
                         << idx << " has published 100000" << " / " << data_it << " data." << endl << endl;
                    last_time = cur_time;
                }
            }
            end = time(nullptr);
            cout << endl << endl
                 << "Thread " << idx << " completes mission, takes " << end - start << " seconds."//, makes " << upt_num << " update."
                 << endl << endl << endl;
        }

        void multi_reg(int subs_size, int idx){
            time_t start, cur_time;
            start = time(nullptr);

            while(true){
                int cur_idx;
                {
                    lock_guard<mutex> lock(this->mtx);
                    cur_idx = this->sub_idx++;
                }
                if(cur_idx >= subs_size){
                    break;
                } else {
                    this->registration(&this->subscribers[cur_idx], idx);
                    if((cur_idx+1) % 100000 == 0){
                        cur_time = time(nullptr);
                        cout << cur_time - start << " seconds passed, 100000 / "
                        << (cur_idx+1) << " subs have been registered." << endl << endl;
                    }
                }
            }
        }

		void registration(Subscriber *s, int idx) const{
			queue<TreeNode*> que;
			TreeNode *cur;
			int relation;

			que.push(this->root[idx]);
			while(!que.empty()){
				cur = que.front();
				que.pop();
				relation = parser->range_relation(cur->start, cur->end, s);
				if(relation == 0) {
					continue;
				} else if(relation == 1 || relation == 3) {
                    if(cur->children == nullptr) {
                        cur->subscribers.push_back(s);
                        continue;
                    }else{
                        for(int i = 0; i < 8; ++i){
                            que.push(cur->children[i]);
                        }
                    }

				} else {
					cur->subscribers.push_back(s);
					continue;
				}
			}
		}
};

int Controller::leaf_th = 0;
#endif
