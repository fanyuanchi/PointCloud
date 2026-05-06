#ifndef RLR_TREETRAIN_DATALOADER_H
#define RLR_TREETRAIN_DATALOADER_H

#include "../index/Geometry.h"

class DataLoader{
public:
    static int pointNum_;
    static int rectNum_;
    static double ratioForSample_;
    static double selectivity_;
    vector<Point> points_{};
    static vector<double> low_, top_;
    std::mt19937 rng_;

    explicit DataLoader(int seed): rng_(seed){}

    static void generateOneSampledRect(const Point &basePoint, Rectangle &out, double sMin, double sMax, std::mt19937 &rng);
    static void generateOneUniformRect(Rectangle &out, double sMin, double sMax, std::mt19937 &rng);

    void loadPoint(const string& pointPath);
    void generateGaussianPoint();

    void loadGaussianRect(vector<Rectangle*>& trainingSet);
    void loadSampleRect(vector<Rectangle*>& trainingSet, const string& pointPath);

    void loadGaussianRectPoint(vector<Rectangle*>& rectSet, vector<Point*>& stabbingQuerySet);
    void loadSampleRectPoint(vector<Rectangle*>& rectSet, vector<Point*>& stabbingQuerySet, const string& pointPath);

    void loadGaussianRectRect(vector<Rectangle*>& rectSet, vector<Rectangle*>& rangeQuerySet);
    void loadSampleRectRect(vector<Rectangle*>& rectSet, vector<Rectangle*>& rangeQuerySet, const string& pointPath);
};

#endif //RLR_TREETRAIN_DATALOADER_H
