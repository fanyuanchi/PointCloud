#include <bits/stdc++.h>
#include "Subscriber.h"
#ifndef PARSER_H
#define PARSER_H

enum Relation {Contain, Contained, Joint, DisJoint};

class Parser{
	public:
		double low_[3]{};
		double range_[3]{};
		int h_max_;
	public:
		Parser(vector<double> &origin, vector<double> &range, int h_max){
			this->h_max_ = h_max;
            for(int dim = 0; dim < 3; ++dim){
                this->low_[dim] = origin[dim];
                this->range_[dim] = range[dim];
            }
		}

		unsigned long long get_morton_code(Point *p) const{
            unsigned long long code = 0;
            double offset[3], semi[3];

            for(int d = 0; d < 3; d++){
                offset[d] = p->cord_[d] - this->low_[d];
                semi[d] = 0;
            }

            int r_k = 2;
            for(int r = 0; r < this->h_max_; r++){
                for(int d = 0; d < 3; d++){
                    code <<= 1;
                    if(offset[d] >= semi[d] + this->range_[d] / r_k){
                        code |= 1;
                        semi[d] += this->range_[d] / r_k;
                    }
                }
                r_k <<= 1;
            }
            p->morton_code_ = code;
            return code;
		}

        static enum Relation range_relation(const double* low1, const double* top1, Subscriber *s){
            double *low2 = s->low_, *top2 = s->top_;
            int r1 = 0, r2 = 0;
            for(auto d = 0; d < 3; d++){
                if(low1[d] <= low2[d] && top1[d] >= top2[d]){
                    ++r1;
                } else if(low1[d] > low2[d] && top1[d] < top2[d]){
                    ++r2;
                } else if(low1[d] >= top2[d] || low2[d] > top1[d]){
                    return DisJoint;
                }
            }

            if(r1 == 3){ //node fully covers the query
                return Contain;
            }else if(r2 == 3){ // node is fully covered by query
                return Contained;
            }else{ // node partly intersects query
                return Joint;
            }
        }

		int get_subcode(Point *p, int resolution) const{
			unsigned long long code = p->morton_code_;
			unsigned long long mask_code = 7;
			unsigned long long sub_code = code >> (3 * (this->h_max_ - resolution - 2));
			return (int)(sub_code & mask_code);
		}
		
};

#endif
