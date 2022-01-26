#pragma once

#include "../helpers/eco_constants.hpp"

static constexpr int UNDEFINED_FD {-1};

enum class Quantity {
    Energy,
    Power,
    Time
};
struct PowerInfo {
    double thermalDesignPower;
    double minPower;
    double maxPower;
    double maxTimeWindow;
};

class MSR {
public:
    MSR() = delete;
    MSR(int core);
    ~MSR();
    uint64_t getEnergyStatus(Domain domain = Domain::PKG);
    double getUnits(Quantity q);
    double getFixedDramUnitsValue(); // some server CPUs use different Power Unit for DRAM
    PowerInfo getPowerInfoForPKG();
    void enableClamping(Domain domain = Domain::PKG);
    void enablePowerCapping(Domain domain = Domain::PKG);
    void disableClamping(Domain domain = Domain::PKG);
    void disablePowerCapping(Domain domain = Domain::PKG);
    bool checkLockedByBIOS();

private:
    int fileDescriptor_ {UNDEFINED_FD};
	void openMSR(int core);
	void writeMSR(int offset, uint64_t value);
	uint64_t readMSR(int offset);
    int getOffsetForPowerLimit(Domain domain = Domain::PKG);
};
