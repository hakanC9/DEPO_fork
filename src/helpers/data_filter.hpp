#pragma once
#include <vector>

class DataFilter {
public:
    DataFilter() = delete;
    DataFilter(int size) :
        filterSize_(size) {}
    double getSMA();
    double getRelativeError();
    double getCleanedRelativeError();

    void storeDataPoint(double dataPoint);
private:
    void shiftActiveIndex();
    double getSum();

    std::vector<double> data_;
    unsigned filterSize_;
    unsigned activeIndex_ {0};
};