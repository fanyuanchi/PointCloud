#include <iostream>
#include "Point.h"
#include "Parser.h"
#include "TreeNode.h"
#include "Controller.h"
#include "Statistics.h"
using namespace std;

void load_data(Point *data, int data_size, Parser* parser, int dimension){
    ifstream file(R"(/home/data/fanyuanchi/Paris_Luxembourg_10CSV.csv)"); // 替换为你的CSV文件路径
    if (!file) {
        cerr << "Failed to open file." << endl;
    }
    string line;
    cout << "Data size: " << data_size << endl;
    getline(file, line);
    for(int i = 0; i < data_size; i++) {
        getline(file, line);
        vector<double> row;
        stringstream ss(line);
        string cell;

        while (getline(ss, cell, ',')) {
            row.push_back(stod(cell));
        }
        data[i].p_assign(row, dimension);
        parser->get_morton_code(&data[i]);

        if((i+1) % 1000000 == 0){
            cout << (i+1) << " data have been loaded." << endl;
        }
    }
    cout << "Data loading completed." << endl;
    file.close();
}
void load_subs(Subscriber *subs, int subs_size, Parser* parser, int dimension){
	ifstream file(R"(/home/data/fanyuanchi/6D_intensive.csv)"); // 替换为你的CSV文件路径
    if (!file) {
        cerr << "Failed to open file." << endl;
    }
    string line;  
	int id;
	cout << "Subs size: " << subs_size << endl;
	getline(file, line);
    for(int i = 0; i < subs_size; i++) {
    	getline(file, line);
        vector<string> row;
        stringstream ss(line);
        string cell;

        while (getline(ss, cell, ',')) {
            row.push_back(cell);
        }
        id = stoi(row.at(0));
        vector<double> start(dimension), end(dimension);
        for(auto d = 0; d < dimension; d++){
            start.at(d) = stod(row.at(1+d)) + parser->origin.at(d);
            end.at(d) = stod(row.at(7+d)) + parser->origin.at(d);
        }
        
		subs[i].s_assign(id, start, end);
		
		if((i+1) % 1000000 == 0){
			cout << (i+1) << " subs have been loaded." << endl;
		}
    }
    cout << "Subs loading completed." << endl;
    file.close();
}

unsigned long long reg_num = 0;
void dfs_reg(TreeNode *cur){
    reg_num += cur->subscribers.size();
    if(cur->children != nullptr){
        for(int i = 0; i < 8; ++i){
            dfs_reg(cur->children[i]);
        }
    }
}

double blr = 0.0;
int path_num = 0;

void dfs_blr(TreeNode *root, int branch_num){
    if(root->children == nullptr){
        if(!root->subscribers.empty() || branch_num != 0){
            path_num++;
            blr += ((double)branch_num+1) / ((double)root->subscribers.size()+1);
        }
    }else{
        for(int i = 0; i < 8; ++i){
            dfs_blr(root->children[i], branch_num+(int)root->subscribers.size());
        }
    }
}

int main(int argc, char** argv) {
    int data_size = 9999999;
	int subs_size = 312500;
	int tree_num = 32;
	Statistics::init(tree_num);
	int level = 7;
    int dimension = 6;

    Point *data = new Point[data_size];
	Subscriber *subs = new Subscriber[subs_size];
	
	vector<double> origin(dimension);
	vector<double> range(dimension);

    origin.at(0) = 26.5862; origin.at(1) = -352.141; origin.at(2) = 36.5479;
    origin.at(3) = 46.4357; origin.at(4) = -348.412; origin.at(5) = 42.8134;

    range.at(0) = 81.1568; range.at(1) = 78.175; range.at(2) = 23.7092;
    range.at(3) = 43.9551; range.at(4) = 55.392; range.at(5) = 0.5695;

	Parser *parser = new Parser(origin, range, level);
	Controller *col = new Controller(parser, tree_num, subs, subs_size, data, data_size);

    load_data(data, data_size, parser, dimension);
    load_subs(subs, subs_size, parser, dimension);

    vector<thread> registers;
    registers.reserve(tree_num);
    for(int i = 0; i < tree_num; ++i){
        registers.emplace_back(&Controller::multi_reg, col, subs_size, i);
    }
    for(auto& registration : registers){
        registration.join();
    }

    for(int i = 0; i < tree_num; ++i){
        Statistics::node_num = 0;
        col->dfs_node(col->root[i]);
        cout << Statistics::node_num << endl << endl;
    }

	for(int i = 0; i < tree_num; ++i){
		cout << Statistics::reg_num[i] << endl;
	}

    vector<thread> publishers;
    publishers.reserve(tree_num);
    for(int i = 0; i < tree_num; ++i){
        publishers.emplace_back(&Controller::publisher, col, i);
    }

//    thread daemon(&Controller::daemon, col, 30);

    for(auto& publisher : publishers){
        publisher.join();
    }

//    daemon.join();

//    dfs_blr(col->root[0], 0);
//    cout << "BLR: " << blr / path_num << endl << endl;

    Statistics::node_num = 0;
    col->dfs_node(col->root[0]);
    cout << Statistics::node_num << endl << endl;

    long long tp = 0;
    int del_num = 0;
    vector<int> vec(8, 0);
    for(int i = 0; i < subs_size; ++i){
        int mesa_num = subs[i].mesa_counter;
        tp += mesa_num;
        if(subs[i].is_delete){
            del_num++;
        }
        if(mesa_num == 0){
            vec.at(0)++;
        }else if(mesa_num < 10){
            vec.at(1)++;
        }else if(mesa_num < 100){
            vec.at(2)++;
        }else if(mesa_num < 1000){
            vec.at(3)++;
        }else if(mesa_num < 10000){
            vec.at(4)++;
        }else if(mesa_num < 100000){
            vec.at(5)++;
        }else if(mesa_num < 1000000){
            vec.at(6)++;
        }else{
            vec.at(7)++;
        }
    }
    for(int & it : vec){
        cout << it << endl;
    }
    cout << endl << endl << "Deletion number: " << del_num << endl << endl;
    cout << endl << endl << "Total publish: " << tp << endl << endl;
    delete []data;
    delete []subs;
    return 0;

}
