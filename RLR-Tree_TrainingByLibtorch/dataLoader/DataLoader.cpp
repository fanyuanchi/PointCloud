#include "DataLoader.h"

int DataLoader::pointNum_ = 10000000;
int DataLoader::rectNum_ = 100000;
double DataLoader::ratioForSample_ = 0.2;
double DataLoader::selectivity_ = 1e-6;

vector<double> DataLoader::low_{};
vector<double> DataLoader::top_{};

void DataLoader::generateOneSampledRect(const Point &basePoint, Rectangle &out, double sMin, double sMax, std::mt19937 &rng){
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    // ---------- Step1: sample selectivity (log-uniform) ----------
    double logSMin = std::log(sMin);
    double logSMax = std::log(sMax);
    double logS = logSMin + uni01(rng) * (logSMax - logSMin);
    double s = std::exp(logS);  // volume ratio

    // ---------- Step2: split volume into 3D extents ----------
    double logAspect[DIM];
    double sum = 0.0;
    for (double & d : logAspect) {
        d = std::log(0.5) + uni01(rng) * std::log(2.0);
        sum += d;
    }
    double extent[DIM];
    for (int dim = 0; dim < DIM; ++dim) {
        double ratio = std::exp(logAspect[dim] - sum / DIM);
        extent[dim] = (DataLoader::top_[dim] - DataLoader::low_[dim]) * pow(s, 1.0 / DIM) * ratio;
    }
    // ---------- Step3: compute low / top ----------
    for (int dim = 0; dim < DIM; ++dim) {
        out.low_[dim] = basePoint.cord_[dim] - extent[dim] * 0.5;
        out.top_[dim] = basePoint.cord_[dim] + extent[dim] * 0.5;
    }
}

void DataLoader::generateOneUniformRect(Rectangle &out, double sMin, double sMax, std::mt19937 &rng){
    // ---------- Step1: perturb center ----------
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    double center[DIM];
    for(int dim = 0; dim < DIM; ++dim){
        double r = uni01(rng) * 0.8 + 0.1;
        center[dim] = DataLoader::low_[dim] + r * (DataLoader::top_[dim] - DataLoader::low_[dim]);
    }
    // ---------- Step2: sample selectivity (log-uniform) ----------
    double logSMin = std::log(sMin);
    double logSMax = std::log(sMax);
    double logS = logSMin + uni01(rng) * (logSMax - logSMin);
    double s = std::exp(logS);  // volume ratio

    // ---------- Step3: split volume into 3D extents ----------
    double logAspect[DIM];
    double sum = 0.0;
    for (double & d : logAspect) {
        d = std::log(0.5) + uni01(rng) * std::log(2.0);
        sum += d;
    }
    double extent[DIM];
    for (int dim = 0; dim < DIM; ++dim) {
        double ratio = std::exp(logAspect[dim] - sum / DIM);
        extent[dim] = (DataLoader::top_[dim] - DataLoader::low_[dim]) * pow(s, 1.0 / DIM) * ratio;
    }
    // ---------- Step4: compute low / top ----------
    for (int dim = 0; dim < DIM; ++dim) {
        out.low_[dim] = center[dim] - extent[dim] * 0.5;
        out.top_[dim] = center[dim] + extent[dim] * 0.5;
    }
}

void DataLoader::loadPoint(const string& pointPath){
    points_.resize(DataLoader::pointNum_);
    ifstream file(pointPath);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + pointPath);
    }

    string line;
    cout << endl << "Data size: " << DataLoader::pointNum_ / 1000 << "k."<< endl;

    for (int idx = 0; idx < DataLoader::pointNum_; ++idx) {
        if (!getline(file, line)) {
            throw std::runtime_error("Unexpected EOF at index " + std::to_string(idx));
        }

        stringstream ss(line);
        string attribute;
        vector<double> cord(DIM);
        int dim = 0;

        while (dim < DIM && getline(ss, attribute, ',')) {
            cord[dim++] = std::stod(attribute);
        }

        if (dim != DIM) {
            throw std::runtime_error("Invalid point format at index " + std::to_string(idx));
        }

        for (int d = 0; d < DIM; ++d) {
            if (cord[d] < DataLoader::low_[d] || cord[d] > DataLoader::top_[d]) {
                throw std::runtime_error("Point out of bound at index " + std::to_string(idx));
            }
        }

        points_[idx].setCord(cord);

        if ((idx + 1) % 2500000 == 0) cout << (idx + 1) / 100000 << "00k points loaded." << endl;
    }
    cout << "Data loading completed." << endl << endl;
    file.close();
}

void DataLoader::generateGaussianPoint(){
    cout << "Gaussian point size: " << DataLoader::rectNum_ / 1000 << "k."<< endl;
    points_.resize(DataLoader::rectNum_);
    vector<double> mean(DIM);
    vector<double> stddev(DIM);
    std::uniform_real_distribution<double> uni01(0.45, 0.55);
    for (int dim = 0; dim < DIM; ++dim) {
        double L = DataLoader::top_[dim] - DataLoader::low_[dim];
        assert(L > 0);
        mean[dim] = DataLoader::low_[dim] + uni01(rng_) * L;
        stddev[dim] = L / 5.0;
    }
    vector<std::normal_distribution<double>> dists;
    dists.reserve(DIM);
    for (int dim = 0; dim < DIM; ++dim) {
        dists.emplace_back(mean[dim], stddev[dim]);
    }
    for(int idx = 0; idx < DataLoader::rectNum_; ++idx){
        vector<double> cord(DIM);
        for(int dim = 0; dim < DIM; ++dim){
            while(true){
                double val = dists[dim](rng_);
                if (val >= DataLoader::low_[dim] && val <= DataLoader::top_[dim]) {
                    cord[dim] = val;
                    break;
                }
            }
        }
        points_[idx].setCord(cord);
        if ((idx + 1) % 25000 == 0) cout << (idx + 1) / 1000 << "k points generated." << endl;
    }
    cout << "Gaussian point generation completed." << endl << endl;
}

void DataLoader::loadGaussianRect(vector<Rectangle*>& trainingSet){
    trainingSet.clear();
    trainingSet.reserve(rectNum_);
    std::mt19937 gen(489563);
    generateGaussianPoint();
    assert(!points_.empty());
    cout << "Gaussian rectangle generation start." << endl;
    double sMin = DataLoader::selectivity_ * 0.9, sMax = DataLoader::selectivity_ * 1.1;
    for (int idx = 0; idx < rectNum_; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, gen);
        trainingSet.push_back(rect);
    }
    cout << "Gaussian rectangle generation completed." << endl;
}

void DataLoader::loadSampleRect(vector<Rectangle*>& trainingSet, const string& pointPath){
    trainingSet.clear();
    trainingSet.reserve(rectNum_);
    loadPoint(pointPath);
    assert(!points_.empty());
    int sampleNum = static_cast<int>(ratioForSample_ * rectNum_);
    std::shuffle(points_.begin(), points_.end(), rng_);

    cout << "Sampled rectangle generation start." << endl;
    double sMin = selectivity_ * 0.9, sMax = selectivity_ * 1.1;
    for (int idx = 0; idx < sampleNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, rng_);
        trainingSet.push_back(rect);
    }

    int uniformNum = rectNum_ - sampleNum;
    for (int idx = 0; idx < uniformNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneUniformRect(*rect, sMin, sMax, rng_);
        trainingSet.push_back(rect);
    }
    std::shuffle(trainingSet.begin(), trainingSet.end(), rng_);
    cout << "Sampled rectangle generation completed." << endl;
}

void DataLoader::loadGaussianRectPoint(vector<Rectangle*>& rectSet, vector<Point*>& stabbingQuerySet){
    rectSet.clear();
    stabbingQuerySet.clear();
    rectSet.reserve(rectNum_);
    stabbingQuerySet.reserve(rectNum_);
    generateGaussianPoint();
    for(int idx = 0; idx < rectNum_; ++idx){
        auto point = new Point(points_[idx]);
        stabbingQuerySet.push_back(point);
    }
    assert(!points_.empty());
    cout << "Gaussian rectangle generation start." << endl;
    double sMin = DataLoader::selectivity_ * 0.9, sMax = DataLoader::selectivity_ * 1.1;
    for (int idx = 0; idx < rectNum_; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }
    cout << "Gaussian rectangle generation completed." << endl;
}

void DataLoader::loadSampleRectPoint(vector<Rectangle*>& rectSet, vector<Point*>& stabbingQuerySet, const string& pointPath){
    rectSet.clear();
    stabbingQuerySet.clear();
    rectSet.reserve(rectNum_);
    stabbingQuerySet.reserve(rectNum_);
    loadPoint(pointPath);
    assert(!points_.empty());
    int sampleNum = static_cast<int>(ratioForSample_ * rectNum_);
    std::shuffle(points_.begin(), points_.end(), rng_);

    for(int idx = 0; idx < rectNum_; ++idx){
        auto point = new Point(points_[idx]);
        stabbingQuerySet.push_back(point);
    }

    cout << "Sampled rectangle generation start." << endl;
    double sMin = selectivity_ * 0.9, sMax = selectivity_ * 1.1;
    for (int idx = 0; idx < sampleNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }

    int uniformNum = rectNum_ - sampleNum;
    for (int idx = 0; idx < uniformNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneUniformRect(*rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }
    std::shuffle(rectSet.begin(), rectSet.end(), rng_);
    cout << "Sampled rectangle generation completed." << endl;
}

void DataLoader::loadGaussianRectRect(vector<Rectangle*>& rectSet, vector<Rectangle*>& rangeQuerySet){
    rectSet.clear();
    rangeQuerySet.clear();
    rectSet.reserve(rectNum_);
    rangeQuerySet.reserve(rectNum_);
    generateGaussianPoint();
    assert(!points_.empty());
    cout << "Gaussian rectangle generation start." << endl;
    double sMin = DataLoader::selectivity_ * 0.9, sMax = DataLoader::selectivity_ * 1.1;
    for (int idx = 0; idx < rectNum_; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }

    double queryMin = 4e-4, queryMax = 6e-4;
    for (int idx = 0; idx < rectNum_; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, queryMin, queryMax, rng_);
        rangeQuerySet.push_back(rect);
    }
    cout << "Gaussian rectangle generation completed." << endl;
}

void DataLoader::loadSampleRectRect(vector<Rectangle*>& rectSet, vector<Rectangle*>& rangeQuerySet, const string& pointPath){
    rectSet.clear();
    rangeQuerySet.clear();
    rectSet.reserve(rectNum_);
    rangeQuerySet.clear();
    loadPoint(pointPath);
    assert(!points_.empty());
    int sampleNum = static_cast<int>(ratioForSample_ * rectNum_);
    std::shuffle(points_.begin(), points_.end(), rng_);

    cout << "Sampled rectangle generation start." << endl;
    double sMin = selectivity_ * 0.9, sMax = selectivity_ * 1.1;
    for (int idx = 0; idx < sampleNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }

    double queryMin = 4e-4, queryMax = 6e-4;
    for (int idx = 0; idx < sampleNum; ++idx){
        auto* rect = new Rectangle();
        generateOneSampledRect(points_[idx], *rect, queryMin, queryMax, rng_);
        rangeQuerySet.push_back(rect);
    }

    int uniformNum = rectNum_ - sampleNum;
    for (int idx = 0; idx < uniformNum; ++idx) {
        auto* rect = new Rectangle();
        generateOneUniformRect(*rect, sMin, sMax, rng_);
        rectSet.push_back(rect);
    }
    std::shuffle(rectSet.begin(), rectSet.end(), rng_);

    for (int idx = 0; idx < uniformNum; ++idx){
        auto* rect = new Rectangle();
        generateOneUniformRect(*rect, queryMin, queryMax, rng_);
        rangeQuerySet.push_back(rect);
    }
    std::shuffle(rangeQuerySet.begin(), rangeQuerySet.end(), rng_);
    cout << "Sampled rectangle generation completed." << endl;
}
