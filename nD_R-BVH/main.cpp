#include <iostream>
#include "Controller.h"
using namespace std;

void load_data(Point *data, int data_size, Parser* parser){
    ifstream file(R"(/home/data/fanyuanchi/Paris_Luxembourg_10CSV.csv)");
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
        data[i].p_assign(row, i);
        parser->get_morton_code(&data[i]);

        if((i+1) % 1000000 == 0){
            cout << (i+1) << " data have been loaded." << endl;
        }
    }
    cout << "Data loading completed." << endl;
    file.close();
}
void load_subs(Subscriber *subs, int subs_size, Parser* parser){
	ifstream file(R"(/home/data/fanyuanchi/6D_min.csv)");
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
        id = stoi(row[0]);
        vector<double> start(DIMS), end(DIMS);
        for(auto d = 0; d < DIMS; d++){
            start[d] = stod(row[1+d]) + parser->origin[d];
            end[d] = stod(row[7+d]) + parser->origin[d];
        }
        
		subs[i].s_assign(id, start, end);
		
		if((i+1) % 1000000 == 0){
			cout << (i+1) << " subs have been loaded." << endl;
		}
    }
    cout << "Subs loading completed." << endl;
    file.close();
}

int main(int argc, char** argv) {
    int data_size = 9999999;
	int subs_size = 1250000;
	int level = 9;

    Point *data = new Point[data_size];
	Subscriber *subs = new Subscriber[subs_size];
	
	vector<double> origin(6);
	vector<double> range(6);

    origin[0] = 26.5862; origin[1] = -352.141; origin[2] = 36.5479;
    origin[3] = 46.4357; origin[4] = -348.412; origin[5] = 42.8134;

    range[0] = 81.1568; range[1] = 78.175; range[2] = 23.7092;
    range[3] = 43.9551; range[4] = 55.392; range[5] = 0.5695;

	Parser *parser = new Parser(origin, range, level);
	Controller *col = new Controller(parser, subs, subs_size, data, data_size);

    load_data(data, data_size, parser);
    load_subs(subs, subs_size, parser);

    vector<thread> registers;
    registers.reserve(PARA);
    for(int i = 0; i < PARA; ++i){
        registers.emplace_back(&Controller::multi_reg, col, subs_size, i);
    }
    for(auto& registration : registers){
        registration.join();
    }

    vector<thread> publishers;
    publishers.reserve(PARA);
    for(int i = 0; i < PARA; ++i){
        publishers.emplace_back(&Controller::publisher, col, i);
    }


    for(auto& publisher : publishers){
        publisher.join();
    }

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
            vec[0]++;
        }else if(mesa_num < 10){
            vec[1]++;
        }else if(mesa_num < 100){
            vec[2]++;
        }else if(mesa_num < 1000){
            vec[3]++;
        }else if(mesa_num < 10000){
            vec[4]++;
        }else if(mesa_num < 100000){
            vec[5]++;
        }else if(mesa_num < 1000000){
            vec[6]++;
        }else{
            vec[7]++;
        }
    }
    for(int & it : vec){
        cout << it << endl;
    }
    cout << endl;
    cout << endl << endl << "Deletion number: " << del_num << endl << endl;
    cout << endl << endl << "Total publish: " << tp << endl << endl;
    delete []data;
    delete []subs;
    return 0;

}
