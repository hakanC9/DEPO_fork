#include "data_filter.hpp"
#include <algorithm>

double DataFilter::getSum() {
    double acc = 0.0;
    for (auto&& dataPoint : data_) {
        acc += dataPoint;
    }
    return acc;
}

double DataFilter::getSMA() {
    return getSum() / data_.size();
}

void DataFilter::storeDataPoint(double dataPoint) {
    if (data_.size() == filterSize_) {
        data_[activeIndex_] = dataPoint;
        shiftActiveIndex();
    } else {
        data_.push_back(dataPoint);
    }
}

void DataFilter::shiftActiveIndex() {
    auto maxIndex = filterSize_ - 1;
    if (activeIndex_ + 1 > maxIndex) {
        activeIndex_ = 0; // reset active index
    } else {
        activeIndex_++; // move active index
    }
}

double DataFilter::getCleanedRelativeError() {
    const auto minmax = std::minmax_element(data_.begin(), data_.end());
    auto min = *minmax.first;
    auto max = *minmax.second;
    auto cleanedSMA = (getSum() - (min + max)) / (data_.size() - 2);
    return (max - min) / cleanedSMA;
}

double DataFilter::getRelativeError() {
    const auto minmax = std::minmax_element(data_.begin(), data_.end());
    return (*minmax.second - *minmax.first) / getSMA();
}