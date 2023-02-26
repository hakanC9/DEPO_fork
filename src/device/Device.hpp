/*
   Copyright 2022, Adam Krzywaniak.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include "../helpers/eco_constants.hpp"
#include <cpucounters.h>

struct AvailablePowerDomains {
    AvailablePowerDomains() {}
    AvailablePowerDomains(bool, bool, bool, bool, bool);
    ~AvailablePowerDomains() {}

    bool pp0_{false}, pp1_{false}, dram_{false}, psys_{false}, fixedDramUnits_{false};
    std::set<PowerCapDomain> availableDomainsSet_;
};

struct RaplDirs
{
    std::vector<std::string> packagesDirs_;
    std::vector<std::string> pp0Dirs_;
    std::vector<std::string> pp1Dirs_;
    std::vector<std::string> dramDirs_;
	const std::string raplBaseDirectory_ {"/sys/class/powercap/intel-rapl:"};
    const std::string pl0dir_ {"constraint_0_power_limit_uw"};
    const std::string pl1dir_ {"constraint_1_power_limit_uw"};
    const std::string window0dir_ {"constraint_0_time_window_us"};
    const std::string window1dir_ {"constraint_1_time_window_us"};
    const std::string isEnabledDir_ {"enabled"};
};

struct RaplDefaults
{
    std::shared_ptr<Constraints> defaultConstrPKG_;
    std::shared_ptr<SubdomainInfo> defaultConstrPP0_;
    std::shared_ptr<SubdomainInfo> defaultConstrPP1_;
    std::shared_ptr<SubdomainInfo> defaultConstrDRAM_;
};

class Device
{
public:
    Device();
    ~Device() {}

    double getPowerLimitInWatts() const;
    void setPowerLimitInMicroWatts(unsigned long limitInMicroW, Domain = PowerCapDomain::PKG);
    std::string getName() const;

    /*
      getDeviceMaxPowerInWatts - used to determine the available power limits range

      returns the maximal available power limit in Watts. For the CPU it assumes that the
      limit is identical to the default power cap. More details in the method's definition.
    */
    int getDeviceMaxPowerInWatts() const;
    void restoreDefaults();
    RaplDefaults getDefaultCaps() const;
    AvailablePowerDomains getAvailablePowerDomains();
    bool isDomainAvailable(Domain);
    void resetPerfCounters();
    double getNumInstructionsSinceReset();
    std::vector<int> pkgToFirstCoreMap_; // TODO getter for this, preferably delete

private:
    void detectCPU();
    void detectPackages();
    void detectPowerCapsAvailability();
    void prepareRaplDirsFromAvailableDomains();
    void readAndStoreDefaultLimits();
    void initPerformanceCounters();
    std::string mapCpuFamilyName(int model) const;
    void setLongTimeWindow(int); // might be useless

    int totalPackages_ {0};
    int totalCores_ {0};
    int model_ {-1};
    pcm::PCM* pcm_;
    AvailablePowerDomains devicePowerProfile_;
    RaplDirs raplDirs_;
    RaplDefaults raplDefaultCaps_;
    static constexpr double DEFAULT_LIMIT {300.0};
    double currentPowerLimitInWatts_ {DEFAULT_LIMIT};
    const std::string defaultLimitsFile_ {"./default_limits_dump.txt"};
    pcm::SystemCounterState sysBeforeState_, sysAfterState_;
    std::vector<pcm::CoreCounterState> beforeState_, afterState_;
    std::vector<pcm::SocketCounterState> dummySocketStates_;
};
