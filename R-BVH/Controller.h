#include <bits/stdc++.h>
#include "TreeNode.h"
#ifndef CONTROLLER_H
#define CONTROLLER_H

#define PARA 32

class Controller{
	public:
		TreeNode **root;
		Parser *parser;
        Subscriber *subscribers;
        Point *data;
        int data_size;
        bool is_cpl[PARA];
        int sub_idx;
		mutex mtx;
        mutex *daemon_mtx;

        static int leaf_th;

	public:
		Controller(Parser *parser, Subscriber* subscribers, Point *data, int data_size){
			this->parser = parser;
            this->subscribers = subscribers;
            this->data = data;
            this->data_size = data_size;
			this->root = new TreeNode*[PARA];
            for(int i = 0; i < PARA; ++i){
                this->root[i] = new TreeNode(parser);
                this->root[i]->split(i);
                this->is_cpl[i] = false;
            }
            this->sub_idx = 0;
            this->daemon_mtx = new mutex[PARA];
            Controller::leaf_th = 64;
        }

        void dfs_node(TreeNode *cur){
            if(cur->children != nullptr){
                for(int i = 0; i < 8; ++i){
                    dfs_node(cur->children[i]);
                }
            }
        }

        void dfs_flush_merge(TreeNode *cur, int idx, time_t cur_time) const {
            if(cur->children != nullptr){
                if(cur->buffer != 0){
                    int left = 0, right = (int)cur->subscribers.size()-1;
                    Subscriber *ls, *rs;
                    while (left < right){
                        while (left < right){
                            ls = cur->subscribers.at(left);
                            if (!ls->is_delete){
                                ls->recv_mesa(cur->buffer);
                                ++left;
                            } else {break;}
                        }
                        if (left >= right){break;}
                        while (left < right){
                            rs = cur->subscribers.at(right);
                            if(rs->is_delete){--right;} else {
                                rs->recv_mesa(cur->buffer);
                                break;
                            }
                        }
                        if (left >= right){break;}
                        swap(cur->subscribers.at(left), cur->subscribers.at(right));
                    }
                    if(left < cur->subscribers.size() && !cur->subscribers.at(left)->is_delete){
                        cur->subscribers.at(left)->recv_mesa(cur->buffer);
                        ++left;}
                    cur->subscribers.erase(cur->subscribers.begin()+left, cur->subscribers.end());
                    cur->buffer = 0;
                    cur->last_upt = cur_time;
                }
                for(int i = 0; i < 8; ++i){
                    dfs_flush_merge(cur->children[i], idx, cur_time);
                }
                if((cur_time - cur->last_upt) > 10){
                    cur->merge(idx, cur_time);
                }
            }else{
                return;
            }
        }

        void daemon(int interval){
            int flag;
            while(true){
                flag = 0;
                time_t cur_time = time(nullptr);
                for(int i = 0; i < PARA; ++i){
                    if(!this->is_cpl[i]){
                        lock_guard<mutex> lock(this->daemon_mtx[i]);
                        this->dfs_flush_merge(this->root[i], i, cur_time);
                        flag = 1;
                    }
                }
                if(flag == 0){
                    break;
                }else{
                    this_thread::sleep_for(chrono::seconds(interval));
                }
            }
        }

        void publisher(int idx){
            Point* p;
            TreeNode *cur;
            int subcode;
//            int upt_num = 0;
//            bool rec;

            time_t cur_time, last_time, start, end;
            start = time(nullptr);
            last_time = start;

            for (int data_it = 0; data_it < this->data_size; data_it++){
                cur_time = time(nullptr);
                p = &this->data[data_it];
                cur = this->root[idx];
//                rec = false;
                {
                    lock_guard<mutex> lock(this->daemon_mtx[idx]);
                    while (true) {
                        cur->last_upt = cur_time;
                        if(cur->children == nullptr){
                            if(cur->height < this->parser->r_max-1 && cur->subscribers.size() >= Controller::leaf_th){
                                cur->split(idx);
//                                rec = true;
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
                }
//                if(rec){
//                    upt_num++;
//                }

                if(data_it > 0 && data_it % 50000 == 0){
                    cur_time = time(nullptr);
                    cout << cur_time - last_time << " / " << cur_time - start << " seconds passed, Thread "
                         << idx << " has published 50000" << " / " << data_it << " data." << endl << endl;
                    last_time = cur_time;
                }
            }

            end = time(nullptr);
            lock_guard<mutex> lock(this->daemon_mtx[idx]);
            this->dfs_flush_merge(this->root[idx], idx, end);
            this->is_cpl[idx] = true;
            end = time(nullptr);
            cout << endl << endl
                 << "Thread " << idx << " completes mission, takes " << end - start << " seconds."//, makes " << upt_num << " split."
                 << endl << endl << endl;
        }

        void multi_reg(int subs_size, int idx){
            time_t start, cur_time;
            start = time(nullptr);

            while(true){
                int cur_idx;
                {
                    lock_guard<mutex> lock(this->mtx);  // 获取互斥锁
                    cur_idx = this->sub_idx++;
                }
                if(cur_idx >= subs_size){
                    break;
                } else {
                    {
                        lock_guard<mutex> lock(this->daemon_mtx[idx]);
                        this->registration(&this->subscribers[cur_idx], idx);
                    }
                    if((cur_idx+1) % 10000 == 0){
                        cur_time = time(nullptr);
                        cout << cur_time - start << " seconds passed, 10000 / "
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
				relation = Parser::range_relation(cur->start, cur->end, s);
				if(relation == 0) {
					continue;
				} else if(relation == 1 || relation == 3) {
                    if(cur->children == nullptr) {
                        cur->add_sub(s, idx);
                        continue;
                    }else{
                        for(int i = 0; i < 8; ++i){
                            que.push(cur->children[i]);
                        }
                    }

				} else {
					cur->add_sub(s, idx);
					continue;
				}
			}
		}

//        void deletion(int id) const{
//            this->subscribers[id].is_delete = true;
//        }
};

int Controller::leaf_th = 0;
#endif
