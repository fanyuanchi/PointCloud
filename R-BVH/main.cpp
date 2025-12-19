#include <iostream>
#include "Controller.h"

void load_data(Point *points, int point_size, Parser* parser){
    ifstream file(); // input the path of point-cloud dataset (.csv) file
    if (!file) cerr << "Failed to open file." << endl;
    string line;
    getline(file, line);
    for(int i = 0; i < point_size; i++) {
        getline(file, line);
        vector<double> row;
        stringstream ss(line);
        string cell;
        while (getline(ss, cell, ',')) row.push_back(stod(cell));
        points[i].Set(row);
        parser->get_morton_code(&points[i]);
    }
    file.close();
}
void load_subs(Subscriber *subscribers, int subscriber_size, Parser* parser){
	ifstream file(); // input the path of subscriber dataset (.csv) file
    if (!file) cerr << "Failed to open file." << endl;
    string line;  
	int id;
	getline(file, line);
    for(int i = 0; i < subscriber_size; i++) {
    	getline(file, line);
        vector<string> row;
        stringstream ss(line);
        string cell;
        while (getline(ss, cell, ',')) row.push_back(cell);
        id = stoi(row.at(0));
        vector<double> start(3), end(3);
        for(auto d = 0; d < 3; d++){
            start[d] = stod(row[1+d]) + parser->low_[d];
            end[d] = stod(row[4+d]) + parser->low_[d];
        }
        subscribers[i].Set(id, start, end);
    }
    file.close();
}

unsigned long long reg_num = 0;
void dfs_reg(TreeNode *cur){
    reg_num += (cur->full_covered_subscribers_.size()+cur->part_covered_subscribers_.size());
    if(cur->children_ != nullptr){
        for(int i = 0; i < 8; ++i){
            dfs_reg(cur->children_[i]);
        }
    }
}

int main(int argc, char** argv) {
    int point_size = // input point-cloud dataset size;
	int subscriber_size = // input subscriber_set size;
	int h_max = 9;

    auto *data = new Point[point_size];
	auto *subs = new Subscriber[subscriber_size];
	
    vector<double> low = // set the lower bound of the global data space;
    vector<double> range = // set the range of global space along X, Y, Z-dimension;
	auto *parser = new Parser(low, range, h_max);
	auto *controller = new Controller(parser,subs, data, point_size);

    load_data(data, point_size, parser);
    load_subs(subs, subscriber_size, parser);

    vector<thread> registers;
    registers.reserve(PARA);
    for(int i = 0; i < PARA; ++i){
        registers.emplace_back(&Controller::multi_reg, controller, subscriber_size, i);
    }
    for(auto& registration : registers){
        registration.join();
    }

    vector<thread> publishers;
    publishers.reserve(PARA);
    for(int i = 0; i < PARA; ++i){
        publishers.emplace_back(&Controller::publisher, controller, i);
    }

    thread daemon(&Controller::daemon, controller, 10);

    for(auto& publisher : publishers){
        publisher.join();
    }

    daemon.join();
    delete []data;
    delete []subs;
    return 0;

}
