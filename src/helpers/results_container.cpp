#include "results_container.hpp"

#include <cmath>

void ResultsContainer::storeOneResult(unsigned index, const FinalPowerAndPerfResult& oneRes) {
    if (index < vec_.size()) {
        vec_[index] = std::move(oneRes);
    } else {
        std::cerr << "[WARNING] Attempt to store unexpected result. Index out of range.\n";
    }
}

EnergyTimeResult ResultsContainer::getAverageResult() const {
    EnergyTimeResult res(0.0, 0.0, 0.0);
    for (auto&& r : vec_) {
        res += r.getEnergyAndTime();
    }
    return res / vec_.size();
}

EnergyTimeResult ResultsContainer::getStdDev() const {
    EnergyTimeResult res(0.0, 0.0, 0.0);
    auto&& av = getAverageResult();
    for (auto&& sample : vec_) {
        res += pow(av - sample.getEnergyAndTime(), 2);
    }
    return sqrt(res / vec_.size());
}

EnergyTimeResult ResultsContainer::getStdDevRel() const {
    auto&& res = getStdDev();
    auto&& av = getAverageResult();
    res.energy_ /= av.energy_;
    res.time_ /= av.time_;
    res.power_ /= av.power_;
    return res;
}
