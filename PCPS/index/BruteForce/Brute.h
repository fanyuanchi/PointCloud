#ifndef PCPS_BRUTE_H
#define PCPS_BRUTE_H
#include "../IIndex.h"

class Brute: public IIndex{
public:
    vector<int> queryIDList_;

    Brute() = default;
    ~Brute() override = default;

    void registerCRQuery(CRQuery* query) override;
    void cancelCRQuery(CRQuery* query) override;
    void publishPoint(Point* point) override;
    bool checkConsistency() const override;
};

#endif //PCPS_BRUTE_H
