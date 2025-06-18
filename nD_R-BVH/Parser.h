#include <bits/stdc++.h>
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

		unsigned long long get_morton_code(Point *p) const{
			unsigned long long code = 0;
            double offset[3], semi[3];

            for(int d = 0; d < 3; d++){
                offset[d] = p->coordinate[d] - this->origin[d];
                semi[d] = 0;
            }

			int r_k = 2;
			for(int r = 0; r < this->r_max; r++){
                for(int d = 0; d < 3; d++){
                    code <<= 1;
                    if(offset[d] >= semi[d] + this->range[d] / r_k){
                        code |= 1;
                        semi[d] += this->range[d] / r_k;
                    }
                }
				r_k <<= 1;
			}
			p->morton_code = code;
			return code;
		}

        int range_relation(const double* s1, const double* e1, Subscriber *s){
			double *s2 = s->start, *e2 = s->end;
			int r1 = 0, r2 = 0;
            for(auto d = 0; d < 3; d++){
                if(s1[d] <= s2[d] && e1[d] >= e2[d]){
                    r1++;
                } else if(s1[d] > s2[d] && e1[d] < e2[d]){
                    r2++;
                } else if(s1[d] > e2[d] || s2[d] > e1[d]){
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


		int get_subcode(Point *p, int resolution) const{
			unsigned long long code = p->morton_code;
			unsigned long long subcode = code >> (3 * (this->r_max - resolution - 2));
			return (int)(subcode & 7);
		}
		
};

#endif
