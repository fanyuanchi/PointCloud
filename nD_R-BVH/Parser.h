#include <bits/stdc++.h>
#include "Point.h"
#include "Subscriber.h"
using namespace std;
#ifndef PARSER_H
#define PARSER_H

class Parser{
	public:
		vector<double> origin;
		vector<double> range;
		int r_max;
	public:
		Parser(const vector<double> &origin, const vector<double> &range, int r_max){
			this->r_max = r_max;
			this->origin.assign(origin.begin(), origin.end());
			this->range.assign(range.begin(), range.end());
		}
		
		//���ڶԵ����������ݵ��������ֵ����Ī�ٱ���
		unsigned long long get_morton_code(Point *p) const{
			unsigned long long code = 0;
            vector<double> offset(3), semi;

            for(auto d = 0; d < 3; d++){
                offset.at(d) = p->coordinate.at(d) - this->origin.at(d);
            }
			semi.assign(3, 0);

			int r_k = 2;
			for(auto r = 0; r < this->r_max; r++){
                for(auto d = 0; d < 3; d++){
                    code <<= 1;
                    if(offset.at(d) >= semi.at(d) + this->range.at(d) / r_k){
                        code |= 1;
                        semi.at(d) += this->range.at(d) / r_k;
                    }
                }
				r_k <<= 1;
			}
			p->morton_code = code;
			return code;
		}
		
		//���ڶ�ĳ���ֱ��ʵ�Ī�������������� 
		pair<vector<double>, vector<double> > decode(unsigned long long code, int resolution) const{
            vector<double> start, end;
            vector<double> offset;
            offset.assign(this->origin.begin(), this->origin.end());

			if(resolution < 0){
				start.assign(this->origin.begin(), this->origin.end());
                for(auto d = 0; d < 3; d++){
                    offset.at(d) += this->range.at(d);
                }
                end.assign(offset.begin(), offset.end());
				return make_pair(start, end);
			}

			int r_k = 2 << resolution;			
			for(auto r = resolution; r >= 0 ; r--){
                for(auto d = 2; d >= 0; d--){
                    if((code & 1) == 1){
                        offset.at(d) += this->range.at(d) / r_k;
                    }
                    code >>= 1;
                }
				r_k >>= 1;
			}
            start.assign(offset.begin(), offset.end());
			r_k = 2 << resolution;
            for(auto d = 0; d < 3; d++){
                offset.at(d) += this->range.at(d) / r_k;
            }
			end.assign(offset.begin(), offset.end());
			return make_pair(start, end);
		}
		
		//�����ж�ĳ��Ī�����Ӧ������������ѯ��Χ��ϵ
		/**
			0	���ཻ
			1  	��Ī�������������ѯ��Χ
			2  	��Ī���뱻������ѯ��Χ���� 
			3  	�ཻ 
		**/
        //s1, e1��ָ���ʾ�������ڵ㷶Χ�Ķ˵�����
		 int range_relation(const vector<double> &s1, const vector<double> &e1, Subscriber *s){
			pair<vector<double>, vector<double> > query_range = s->get_query_range();
			//s2, e2��ָ���ʾ������ѯ��Χ�Ķ˵�����
            vector<double> s2 = query_range.first; vector<double> e2 = query_range.second;
			int r1 = 0, r2 = 0;

            for(auto d = 0; d < 3; d++){
                if(s1.at(d) <= s2.at(d) && e1.at(d) >= e2.at(d)){
                    r1++;
                } else if(s1.at(d) > s2.at(d) && e1.at(d) < e2.at(d)){
                    r2++;
                } else if(s1.at(d) > e2.at(d) || s2.at(d) > e1.at(d)){
                    return 0;
                }
            }

			if(r1 == 3){
				return 1;
			}else if(r2 == 3){
				return 2;
			}else{
				return 3;
			}		
		}

		//���ڻ�õ������������ڵ�ǰ�ֱ��ʵ����� 
		int get_subcode(Point *p, int resolution) const{
			unsigned long long code = p->morton_code;
			unsigned long long mask_code = 7;
			unsigned long long subcode = code >> (3 * (this->r_max - resolution - 2));
			return (int)(subcode & mask_code);
		}
		
};

#endif
