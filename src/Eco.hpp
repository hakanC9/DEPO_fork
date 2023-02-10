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
#include <cpucounters.h>
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
    double getK() { return cfg_.k_; } // temporary getter until Eco is reorganised
    void setCustomK(double k) { cfg_.k_ = k; } // temporary setter until Eco is reorganised
    int getNumIterations() { return cfg_.numIterations_; }
    std::string getResultFileName() const { return outResultFileName_; }
    std::string getPowerLogFileName() const { return outPowerFileName_; }
    EcoApi() = default;
    int readLimitFromFile (std::string);
    void writeLimitToFile (std::string , int);
    virtual ~EcoApi() = default;
  protected:
    ParamsConfig cfg_; // stores defaults values of params or reads it from params.conf
    std::string outResultFileName_;
    std::string outPowerFileName_;
};

class Eco : public EcoApi
{
  public:
    void setPowerCap(int, Domain = PowerCapDomain::PKG);
    void referenceRunWithoutCaps(char* const*);
    void runAppForEachPowercap(char* const*, BothStream&, Domain = PowerCapDomain::PKG);
    void idleSample(int idleTimeS) override;
    FinalPowerAndPerfResult runAppWithLinearSearch(char* const*,
                                                   TargetMetric = TargetMetric::MIN_E);
    FinalPowerAndPerfResult runAppWithSampling(char* const*, int = 1);
    FinalPowerAndPerfResult runAppWithGoldenSectionSearch(char* const*,
                                                          TargetMetric = TargetMetric::MIN_E);
    FinalPowerAndPerfResult runAppWithSearch(char* const*, TargetMetric, SearchType, int = 1) override;
    void storeReferenceRun(FinalPowerAndPerfResult&);
    void plotPowerLog() override;
    std::string getCpuName() { return cpu_.getCPUname(); }
    void staticEnergyProfiler(char* const* argv, BothStream& stream);

    Eco();
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
    pcm::SystemCounterState SysBeforeState, SysAfterState;
    std::vector<pcm::CoreCounterState> BeforeState, AfterState;
    std::vector<pcm::SocketCounterState> DummySocketStates;
    pcm::PCM* m;
    DataFilter filter2order_;
    Device cpu_;
    CrossDomainQuantity idleAvPow;
    std::set<PowerCapDomain> availableDomains;
    double pprevSMA_ {0.0}, prevSMA_ {0.0};
    bool optimizationTrigger_ {false};
    // -----
    // class DirBuilder
    const std::string pl0dir {"constraint_0_power_limit_uw"};
    const std::string pl1dir {"constraint_1_power_limit_uw"};
    const std::string window0dir {"constraint_0_time_window_us"};
    const std::string window1dir {"constraint_1_time_window_us"};
    const std::string isEnabledDir {"enabled"};
    std::string raplBaseDirectory {"/sys/class/powercap/intel-rapl:"};
    std::vector<std::string> packagesDirs_;
    std::vector<std::string> pp0Dirs_;
    std::vector<std::string> pp1Dirs_;
    std::vector<std::string> dramDirs_;
    // ---- 
    DeviceState devStateGlobal_;
    DeviceState devStateLocal_;
    std::vector<FinalPowerAndPerfResult> oneSeriesResultVec;
    std::shared_ptr<Constraints> defaultConstrPKG;
    std::shared_ptr<SubdomainInfo> defaultConstrPP0;
    std::shared_ptr<SubdomainInfo> defaultConstrPP1;
    std::shared_ptr<SubdomainInfo> defaultConstrDRAM;
    // struct with data which may be dumped
    const std::string defaultLimitsFile {"./default_limits_dump.txt"};
    WatchdogStatus defaultWatchdog;
    // --
    std::ofstream outPowerFile;
    std::chrono::high_resolution_clock::time_point startTime_;

    void initPcmCounters();
    void storeDataPointToFilters(double);
    double getFilteredPower();
    void modifyWatchdog(WatchdogStatus);
    WatchdogStatus readWatchdog();
    void raplSample();
    void restoreDefaults();
    std::string generateUniqueResultDir();
    std::vector<int> generateVecOfPowerCaps(Domain = PowerCapDomain::PKG);
    void singleAppRunAndPowerSample(char* const*);
    FinalPowerAndPerfResult multipleAppRunAndPowerSample(char* const*, int);
    void checkIdlePowerConsumption();
    void localPowerSample(int);
    PowAndPerfResult checkPowerAndPerformance(int);
    void readAndStoreDefaultLimits();
    PowAndPerfResult setCapAndMeasure(int, int);
    void justSample(int timeS);
    void setLongTimeWindow(int);
    void reportResult(double = 0.0, double = 0.0);
    void waitPhase(int&, int);
    int testPhase(int&, int&, TargetMetric, SearchType, PowAndPerfResult&);
    void execPhase(int, int&, int, PowAndPerfResult&);
    void mainAppProcess(char* const*, int&);
    int& adjustHighPowLimit(PowAndPerfResult, int&);
    int linearSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric);
    int goldenSectionSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric);

    // TODO: temporary solution
    double currentPowerCapPKG_ {DEFAULT_LIMIT}; // shall be moved to power management interface along with setPoweCap logic
    static constexpr int FIRST_RUN_MULTIPLIER {3};
    static constexpr double DEFAULT_LIMIT {300.0};
};
