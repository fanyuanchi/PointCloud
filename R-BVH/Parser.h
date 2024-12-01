#include <bits/stdc++.h>
#include "Point.h"
#include "Subscriber.h"
using namespace std;
#ifndef PARSER_H
#define PARSER_H

class Parser{
	public:
		double origin_x, origin_y, origin_z;
		double range_x, range_y, range_z;
		int r_max;
	public:
		Parser(vector<double> &origin, vector<double> &range, int r_max){
			this->r_max = r_max;
			this->origin_x = origin.at(0); this->origin_y = origin.at(1); this->origin_z = origin.at(2);
			this->range_x = range.at(0); this->range_y = range.at(1); this->range_z = range.at(2);
		}
		Parser(const Parser &p){
			this->r_max = p.r_max;
			this->origin_x = p.origin_x; this->origin_y = p.origin_y; this->origin_z = p.origin_z;
			this->range_x = p.range_x; this->range_y = p.range_y; this->range_z = p.range_z;
		}
		
		//用于对单个点云数据进行莫顿编码	
		unsigned long long get_morton_code(Point *p) const{
			unsigned long long code = 0;
			double offset_x = p->x - this->origin_x, offset_y = p->y - this->origin_y, offset_z = p->z - this->origin_z;
			double semi_ox = 0, semi_oy = 0, semi_oz = 0;

			int r_k = 2;
			for(auto r = 0; r < this->r_max; r++){
				code <<= 1;
				if(offset_x >= semi_ox + this->range_x / r_k){
					code |= 1;
					semi_ox += this->range_x / r_k;
				}
				code <<= 1;
				if(offset_y >= semi_oy + this->range_y / r_k){
					code |= 1;
					semi_oy += this->range_y / r_k;
				}
				code <<= 1;
				if(offset_z >= semi_oz + this->range_z / r_k){
					code |= 1;
					semi_oz += this->range_z / r_k;
				}
				r_k <<= 1;
			}
			p->morton_code = code;
			// cout << p->p_string() << endl << endl;
			return code;
		}
		
		//用于对某个分辨率的莫顿码进行区域解码 
		pair<Point*, Point*> decode(unsigned long long code, int resolution) const{
			Point *ptr_p1, *ptr_p2;
			if(resolution < 0){
				ptr_p1 = new Point(this->origin_x, this->origin_y, this->origin_z);
				ptr_p2 = new Point(this->range_x, this->range_y, this->range_z);
				return make_pair(ptr_p1, ptr_p2);
			}
			double offset_x = this->origin_x, offset_y = this->origin_y, offset_z = this->origin_z;
			int r_k = 2 << resolution;			
			for(auto r = resolution; r >= 0 ; r--){
				if((code & 1) == 1){
					offset_z += this->range_z / r_k;
				}
				code >>= 1;
				if((code & 1) == 1){
					offset_y += this->range_y / r_k;
				}
				code >>= 1;
				if((code & 1) == 1){
					offset_x += this->range_x / r_k;
				}
				code >>= 1;
				r_k >>= 1;
			}
			ptr_p1 = new Point(offset_x, offset_y, offset_z);
			r_k = 2 << resolution;
			offset_x += this->range_x / r_k; offset_y += this->range_y / r_k; offset_z += this->range_z / r_k;
			ptr_p2 = new Point(offset_x, offset_y, offset_z);
			return make_pair(ptr_p1, ptr_p2);
		}
		
		//用于判断某个莫顿码对应区域与正交查询范围关系
		/**
			0	不相交
			1  	该莫顿码包含正交查询范围
			2  	该莫顿码被正交查询范围包含 
			3  	相交 
		**/ 
		static int range_relation(Point *p1, Point *p2, Subscriber *s){ //p1、p2是指向表示订阅树节点范围的端点坐标
			pair<Point*, Point*> query_range = s->get_query_range();
			//p3、p4是指向表示正交查询范围的端点坐标
			Point *p3 = query_range.first; Point *p4 = query_range.second; 
			int r1 = 0, r2 = 0;
			if(p1->x <= p3->x && p2->x >= p4->x){
				r1++;
			}else if(p1->x > p3->x && p2->x < p4->x){
				r2++;
			}else if(p1->x >= p4->x || p3->x >= p2->x){
				return 0;
			}		
			if(p1->y <= p3->y && p2->y >= p4->y){
				r1++;
			}else if(p1->y > p3->y && p2->y < p4->y){
				r2++;
			}else if(p1->y >= p4->y || p3->y >= p2->y){
				return 0;
			}	
			if(p1->z <= p3->z && p2->z >= p4->z){
				r1++;
			}else if(p1->z > p3->z && p2->z < p4->z){
				r2++;
			}else if(p1->z >= p4->z || p3->z >= p2->z){
				return 0;
			}
			if(r1 == 3){
				return 1;
			}else if(r2 == 3){
				return 2;
			}else{
				return 3;
			}		
		}

		//用于获得单个点云数据在当前分辨率的子码 
		int get_subcode(Point *p, int resolution) const{
			unsigned long long code = p->morton_code;
			unsigned long long mask_code = 7;
			unsigned long long subcode = code >> (3 * (this->r_max - resolution - 2));
			return (int)(subcode & mask_code);
		}
		
};

#endif
