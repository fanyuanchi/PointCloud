#include <iostream>
#include "Controller.h"

void load_data(Point *data, int data_size, Parser* parser){
    ifstream file(R"(/home/data/fanyuanchi/01.csv)");
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
        data[i].Set(row);
        parser->get_morton_code(&data[i]);

        if((i+1) % 1000000 == 0){
            cout << (i+1) << " data have been loaded." << endl;
        }
    }
    cout << "Data loading completed." << endl;
    file.close();
}
void load_subs(Subscriber *subs, int subs_size, Parser* parser){
	ifstream file(R"(/home/data/fanyuanchi/Subscribers_intensive.csv)");
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
        vector<double> start(3), end(3);
        for(auto d = 0; d < 3; d++){
            start[d] = stod(row[1+d]) + parser->origin[d];
            end[d] = stod(row[4+d]) + parser->origin[d];
        }
        
		subs[i].Set(id, start, end);
		
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
	int data_size = 14644635;
//    int data_size = 14295781;
	int subs_size = 1250000;
	int tree_num = 8;
	int level = 9;

    Point *data = new Point[data_size];
	Subscriber *subs = new Subscriber[subs_size];
	
	vector<double> origin(3);
	vector<double> range(3);
	origin.at(0) = 0; origin.at(1) = 0; origin.at(2) = 0;
	range.at(0) = 1945308; range.at(1) = 1992782; range.at(2) = 253803;
//    origin.at(0) = -129894; origin.at(1) = -184620; origin.at(2) = -2252;
//    range.at(0) = 301713; range.at(1) = 386952; range.at(2) = 97686;
	Parser *parser = new Parser(origin, range, level);
	Controller *col = new Controller(parser,subs, data, data_size);

    load_data(data, data_size, parser);
    load_subs(subs, subs_size, parser);

    vector<thread> registers;
    registers.reserve(tree_num);
    for(int i = 0; i < tree_num; ++i){
        registers.emplace_back(&Controller::multi_reg, col, subs_size, i);
    }
    for(auto& registration : registers){
        registration.join();
    }

    vector<thread> publishers;
    publishers.reserve(tree_num);
    for(int i = 0; i < tree_num; ++i){
        publishers.emplace_back(&Controller::publisher, col, i);
    }

    thread daemon(&Controller::daemon, col, 30);

    for(auto& publisher : publishers){
        publisher.join();
    }

    daemon.join();

    long long tp = 0;
    int del_num = 0;
    vector<int> vec(9, 0);
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
        }else if(mesa_num < 10000000){
            vec.at(7)++;
        }else{
            vec.at(8)++;
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
