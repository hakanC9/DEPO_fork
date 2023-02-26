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

#include <chrono>
#include <memory>
#include <set>
// Workaround: below two has to be included in such order to ensure no warnings
//             about macro redefinitions. Some of the macros in Rapl.hpp are already
//             defined in types.h included by cpucounters.h but not all of them.
#include "device/device_state.hpp"
//----------------------------------------------------------------------------------
#include "helpers/power_and_perf_result.hpp"
#include "helpers/eco_constants.hpp"
#include "helpers/final_power_and_perf_result.hpp"
#include "helpers/params_config.hpp"
#include "helpers/data_filter.hpp"
#include "helpers/both_stream.hpp"

template <class F>
inline auto measureDuration(F&& fun)
{
    const auto start = std::chrono::high_resolution_clock::now();
    fun();
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
}

static inline
void validateExecStatus(int status) {
    if (status) {
        std::cerr << "execv failed with error " << errno
                  << " " << strerror(errno) << std::endl;
    }
}

class EcoApi
{
  public:
    virtual void idleSample(int) = 0;
    virtual FinalPowerAndPerfResult runAppWithSampling(char* const*, int) = 0;
    virtual FinalPowerAndPerfResult runAppWithSearch(char* const*, TargetMetric, SearchType, int) = 0;
    virtual void plotPowerLog() = 0;
    virtual std::string getDeviceName() const = 0;

    double getK() { return cfg_.k_; } // temporary getter until Eco is reorganised
    void setCustomK(double k) { cfg_.k_ = k; } // temporary setter until Eco is reorganised
    int getNumIterations() { return cfg_.numIterations_; }
    std::string getResultFileName() const { return outResultFileName_; }
    std::string getPowerLogFileName() const { return outPowerFileName_; }
    EcoApi() = default;
    virtual ~EcoApi() = default;
  protected:
    ParamsConfig cfg_; // stores defaults values of params or reads it from params.conf
    std::string outResultFileName_;
    std::string outPowerFileName_;
};

class Eco : public EcoApi
{
  public:
    void idleSample(int idleTimeS) override;
    FinalPowerAndPerfResult runAppWithSampling(char* const*, int = 1) override;
    FinalPowerAndPerfResult runAppWithSearch(char* const*, TargetMetric, SearchType, int = 1) override;
    void plotPowerLog() override;
    std::string getDeviceName() const override { return device_->getName(); }

    void referenceRunWithoutCaps(char* const*);
    void runAppForEachPowercap(char* const*, BothStream&, Domain = PowerCapDomain::PKG);
    FinalPowerAndPerfResult runAppWithLinearSearch(char* const*,
                                                   TargetMetric = TargetMetric::MIN_E);
    FinalPowerAndPerfResult runAppWithGoldenSectionSearch(char* const*,
                                                          TargetMetric = TargetMetric::MIN_E);
    void storeReferenceRun(FinalPowerAndPerfResult&);
    void staticEnergyProfiler(char* const* argv, BothStream& stream);

    Eco(std::shared_ptr<Device>);
    virtual ~Eco();

  protected:
    enum class FilterType {
        SMA50,
        SMA100,
        SMA200,
        SMA_1_S,
        SMA_2_S,
        SMA_5_S,
        SMA_10_S,
        SMA_20_S
    };
  private:
    std::map<FilterType, DataFilter> smaFilters_;
    FilterType activeFilter_ {FilterType::SMA100};
    DataFilter filter2order_;
    std::shared_ptr<Device> device_;
    CrossDomainQuantity idleAvPow_;
    double pprevSMA_ {0.0}, prevSMA_ {0.0};
    bool optimizationTrigger_ {false};

    DeviceStateAccumulator devStateGlobal_;
    DeviceStateAccumulator devStateLocal_;
    std::vector<FinalPowerAndPerfResult> fullAppRunResultsContainer_;

    WatchdogStatus defaultWatchdog;
    std::ofstream outPowerFile;

    void storeDataPointToFilters(double);
    double getFilteredPower();
    void modifyWatchdog(WatchdogStatus);
    WatchdogStatus readWatchdog();
    void raplSample();
    std::string generateUniqueResultDir();
    std::vector<int> generateVecOfPowerCaps(Domain = PowerCapDomain::PKG);
    void singleAppRunAndPowerSample(char* const*);
    FinalPowerAndPerfResult multipleAppRunAndPowerSample(char* const*, int);
    void checkIdlePowerConsumption();
    void localPowerSample(int);
    PowAndPerfResult checkPowerAndPerformance(int);
    PowAndPerfResult setCapAndMeasure(int, int);
    void justSample(int timeS);
    void reportResult(double = 0.0, double = 0.0);
    void waitPhase(int&, int);
    int testPhase(int&, int&, TargetMetric, SearchType, PowAndPerfResult&);
    void execPhase(int, int&, int, PowAndPerfResult&);
    void mainAppProcess(char* const*, int&);
    int& adjustHighPowLimit(PowAndPerfResult, int&);
    int linearSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric);
    int goldenSectionSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric);

    static constexpr int FIRST_RUN_MULTIPLIER {3};
};
