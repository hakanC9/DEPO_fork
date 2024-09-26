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
#include <optional>
#include <cpucounters.h>
#include "../power_if/Rapl.hpp"
#include "../helpers/eco_constants.hpp"

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
    Device() {}
    virtual ~Device() = default;
    virtual std::string getName() const = 0;
    // virtual void restoreDefaults() = 0;
    // virtual int getDeviceMaxPowerInWatts() const = 0;
    virtual std::pair<unsigned, unsigned> getMinMaxLimitInWatts() const = 0;
    virtual double getPowerLimitInWatts() const = 0;
    virtual void setPowerLimitInMicroWatts(unsigned long limitInMicroW) = 0;
    // virtual RaplDefaults getDefaultCaps() const = 0; // TODO: remove dependency on RAPL
    virtual void reset() = 0;
    virtual unsigned long long int getPerfCounter() const = 0;
    virtual double getCurrentPowerInWatts(std::optional<Domain>) const = 0;
    // virtual void readAndStoreDefaultLimits() = 0;
    virtual void restoreDefaultLimits() = 0;
    virtual std::string getDeviceTypeString() const = 0;
    /*
      triggerPowerApiSample - used to trigger next sample from Power Management API

      This method is required for generalization of DeviceStateAccumulator.
      Intel RAPL API depends on explicit triggering of next energy counter reads while
      other vendors (like, e.g., NVIDIA in NVML) do it automatically in the library.
      This makes defining this method OPTIONAL for most of vendors - the method definition
      may be just left empty. For Intel it needs to have Rapl::sample() method call.
    */
    virtual void triggerPowerApiSample() = 0;

private:
};


class IntelDevice : public Device
{
public:
    IntelDevice();
    virtual ~IntelDevice() = default;

    double getPowerLimitInWatts() const override;
    void setPowerLimitInMicroWatts(unsigned long limitInMicroW) override;
    std::string getName() const override;
    void reset() override;
    double getCurrentPowerInWatts(std::optional<Domain> = std::nullopt) const override;
    void triggerPowerApiSample() override;
    unsigned long long int getPerfCounter() const override;

    /*
      getMinMaxLimitInWatts - used to determine the available power limits range

      returns the maximal available power limit in Watts. For the CPU it assumes that the
      limit is identical to the default power cap. More details in the method's definition.
      For Intel CPU it returns 0 as minimal value. In the future it might be fixed.
    */
    std::pair<unsigned, unsigned> getMinMaxLimitInWatts() const override;
    std::string getDeviceTypeString() const override { return "cpu"; };

    void readAndStoreDefaultLimits();
    void restoreDefaultLimits() override;
    // RaplDefaults getDefaultCaps() const;
    AvailableRaplPowerDomains getAvailablePowerDomains();
    bool isDomainAvailable(Domain);
    double getNumInstructionsSinceReset() const;
    std::vector<int> getPkgToFirstCoreMap() const { return pkgToFirstCoreMap_; }

private:
    void detectCPU();
    void detectPackages();
    void detectPowerCapsAvailability();
    void prepareRaplDirsFromAvailableDomains();
    // void readAndStoreDefaultLimits();
    void initPerformanceCounters();
    std::string mapCpuFamilyName(int model) const;
    void setLongTimeWindow(int); // might be useless
    void initRaplObjectsForEachPKG();
    void checkIdlePowerConsumption();

    int totalPackages_ {0};
    int totalCores_ {0};
    int model_ {-1};
    pcm::PCM* pcm_;
    AvailableRaplPowerDomains devicePowerProfile_;
    RaplDirs raplDirs_;
    RaplDefaults raplDefaultCaps_;
    // static constexpr double DEFAULT_LIMIT {300.0};
    double currentPowerLimitInWatts_;
    double idlePowerConsumption_;
    const std::string defaultLimitsFile_ {"./default_limits_dump.txt"};
    std::vector<int> pkgToFirstCoreMap_;
    std::vector<Rapl> raplVec_;
    pcm::SystemCounterState sysBeforeState_;
    std::vector<pcm::CoreCounterState> beforeState_;
};
