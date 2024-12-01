#include <bits/stdc++.h>
using namespace std;
#ifndef STATISTICS_H
#define STATISTICS_H

class Statistics{
	public:
		static int node_num;
		static long long* total_publ_num;
		static int* reg_num;
        static int* upt_num;
		static int tree_num;

	public:
		static void init(int tree_num){
			Statistics::tree_num = tree_num;
            Statistics::upt_num = new int[tree_num];
			Statistics::reg_num = new int[tree_num];
			Statistics::total_publ_num = new long long[tree_num];
			for(int i = 0; i < tree_num; ++i){
                Statistics::upt_num[i] = 0;
				Statistics::reg_num[i] = 0;
				Statistics::total_publ_num[i] = 0;
			}
		}
};

int Statistics::node_num = 0;
int Statistics::tree_num = 0;
int* Statistics::reg_num = nullptr;
int* Statistics::upt_num = nullptr;
long long* Statistics::total_publ_num = nullptr;


#endif
