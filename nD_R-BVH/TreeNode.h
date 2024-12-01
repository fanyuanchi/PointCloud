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
        vector<double> start;
        vector<double> end;
        vector<Subscriber*> subscribers;
		TreeNode** children;
		 
	public:
		static int data_max_size;
		
		TreeNode(unsigned long long morton_code, int height, Parser* parser){
			this->morton_code = morton_code;
			this->height = height;
			
			pair<vector<double>, vector<double> > range = parser->decode(morton_code, height);
			this->start.assign(range.first.begin(), range.first.end());
			this->end.assign(range.second.begin(), range.second.end());
			
			this->children = nullptr;
		}
		
		
		~TreeNode(){
			for(int i = 0; i < 8; ++i){
				delete this->children[i];
			}
			delete[] this->children;
		}
		
		//���ι����������ھ�ȷ�ж�һ�����Ƿ��ڸö��Ĳ�ѯ��Χ 
		/**
		false: �õ������ݲ��ڶ��Ĳ�ѯ��Χ��
		true:  �����ڶ��Ĳ�ѯ��Χ�� 
		**/
		bool full_filter(Point *p, Subscriber *s){
			pair<vector<double>, vector<double> > query_range = s->get_query_range();
            vector<double> start = query_range.first;
            vector<double> end = query_range.second;
            int k = start.size();

            for(auto d = 0; d < k; d++){
                if(p->coordinate.at(d) < start.at(d) || p->coordinate.at(d) > end.at(d)){
                    return false;
                }
            }
            return true;
		}

    bool part_filter(Point *p, Subscriber *s){
        pair<vector<double>, vector<double> > query_range = s->get_query_range();
        vector<double> start = query_range.first;
        vector<double> end = query_range.second;
        int k = start.size();

        for(auto d = 3; d < k; d++){
            if(p->coordinate.at(d) < start.at(d) || p->coordinate.at(d) > end.at(d)){
                return false;
            }
        }
        return true;
    }
		
		// �ڵ����(������һ��)
		// �����ɿյ��ӽڵ㣬Ȼ�󽫸ýڵ����ݸ��Ƶ���Ӧ�ӽڵ��� 
		void split(Parser *parser, int idx){
			this->children = new TreeNode*[8];
			for(int i = 0; i < 8; ++i){
				unsigned long long subcode = (this->morton_code << 3) + i;
				this->children[i] = new TreeNode(subcode, this->height+1, parser);
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
                            child->add_sub(s, idx);
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

		void add_sub(Subscriber *s, int idx) {
			this->subscribers.push_back(s);
			Statistics::reg_num[idx]++;
		}
		
		void publish(Point *p) {
            if (this->children == nullptr) { //Ҷ�ڵ㷢��ǰ�����ι���
                for(auto s: this->subscribers){
                    if(this->full_filter(p, s)){
                        s->recv_mesa();
                    }
                }
            } else { //�м�ڵ��Ȼ��������������
                for(auto s: this->subscribers){
                    if(this->part_filter(p, s)){
                        s->recv_mesa();
                    }
//                    s->recv_mesa();
                }
            }
        }
};
#endif
