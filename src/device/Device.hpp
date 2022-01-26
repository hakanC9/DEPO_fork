#pragma once

#include <string>
#include <vector>
#include <memory>

struct AvailablePowerDomains {
    AvailablePowerDomains() {}
    AvailablePowerDomains(bool, bool, bool, bool, bool);
    ~AvailablePowerDomains() {}

    bool pp0_{false}, pp1_{false}, dram_{false}, psys_{false}, fixedDramUnits_{false};
};

class Device {
public:
    Device();
    ~Device() {}
    int getNumPackages();
    int getModel();
    std::string getCPUname();
    AvailablePowerDomains getAvailablePowerDomains();
    std::vector<int> pkgToFirstCoreMap_; // TODO getter for this, preferably delete

private:
    void detectCPU();
    void detectPackages();
    void detectPowerCapsAvailability();
    std::string mapCpuFamilyName(int& model);

    int totalPackages_ {0};
    int totalCores_ {0};
    int model_ {-1};
    AvailablePowerDomains devicePowerProfile_;
};
