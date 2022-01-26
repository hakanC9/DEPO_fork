#pragma once

#include "final_power_and_perf_result.hpp"

#include <vector>

class ResultsContainer {
public:
    ResultsContainer(int size) { vec_.resize(size); }
    ResultsContainer() = delete;
    ~ResultsContainer() = default;
    EnergyTimeResult getAverageResult() const;
    EnergyTimeResult getStdDev() const;
    EnergyTimeResult getStdDevRel() const;
    void storeOneResult(unsigned index, const FinalPowerAndPerfResult& oneRes);

private:
    std::vector<FinalPowerAndPerfResult> vec_;
};
