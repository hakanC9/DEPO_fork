#include "device_state.hpp"

#include <set>

DeviceState::DeviceState() {
    for (auto&& cpuCore : cpu.pkgToFirstCoreMap_) {
        raplVec_.emplace_back(cpuCore, cpu.getAvailablePowerDomains());
    }
}

void DeviceState::resetDevice() {
    for (auto&& rapl : raplVec_) {
        rapl.reset();
    }
}

void DeviceState::sample() {
    for (auto&& rapl : raplVec_) {
        rapl.sample();
    }
}

double DeviceState::getCurrentPower(Domain d) {
    double result = 0.0;
    for (auto&& rapl : raplVec_) {
        result += rapl.getCurrentPower()[d];
    }
    return result;
}

double DeviceState::getPkgMaxPower() {
    return raplVec_.front().pkg_max_power() * raplVec_.size();
    //above may cause problems when vector is empty or when two different CPUs are in one device
}

double DeviceState::getTotalAveragePower(Domain d) {
    double result = 0.0;
    for (auto&& rapl : raplVec_) {
        result += rapl.getAveragePower()[d];
    }
    return result;
}

double DeviceState::getTotalTime() {
    std::set<double> timeSet;
    for (auto&& rapl : raplVec_) {
        timeSet.insert(rapl.total_time());
    }
    return *timeSet.rbegin(); // set is sorted containter
}

double DeviceState::getTotalEnergy(Domain d) {
    double result = 0.0;
    for (auto&& rapl : raplVec_) {
        result += rapl.getTotalEnergy()[d];
    }
    return result;
}

std::vector<double> DeviceState::getTotalEnergyVec(Domain d) {
    std::vector<double> result;
    for (auto&& rapl : raplVec_) {
        result.push_back(rapl.getTotalEnergy()[d]);
    }
    return result;
}