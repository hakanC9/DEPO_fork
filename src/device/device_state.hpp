#pragma once

#include "Device.hpp"
#include "../power_if/Rapl.hpp"

class DeviceState {
public:
    DeviceState();
    ~DeviceState() {}
    double getTotalAveragePower(Domain d);
    double getTotalEnergy(Domain d);
    std::vector<double> getTotalEnergyVec(Domain d);
    double getTotalTime();
    void sample();
    void resetDevice();
    double getPkgMaxPower();
    double getCurrentPower(Domain d);

private:
    Device cpu;
    std::vector<Rapl> raplVec_;
};