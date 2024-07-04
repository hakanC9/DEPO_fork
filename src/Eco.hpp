/*
   Copyright 2022-2024, Adam Krzywaniak.

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
#include <optional>
#include <functional>
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

enum class TriggerType
{
  NO_TUNING,
  SINGLE_IMMEDIATE_TUNING,
  SINGLE_TUNING_WITH_WAIT,
  PERIODIC_IMMEDIATE_TUNING,
  PERIODIC_TUNING_WITH_WAIT,
  EXTERNAL_TRIGGER_FOR_TUNING,
};

class Trigger
{
  public:
    Trigger() = delete;
    Trigger(TriggerType tt) :
      type_(tt) {}
    ~Trigger() = default;

    bool checkTunningPhaseTrigger()
    {
      return true;
    }

  private:
    TriggerType type_;
};


class EcoApi
{
  public:
    virtual void idleSample(int) = 0;
    virtual FinalPowerAndPerfResult runAppWithSampling(char* const*, int) = 0;
    virtual FinalPowerAndPerfResult runAppWithSearch(char* const*, TargetMetric, SearchType, int) = 0;
    virtual void plotPowerLog(std::optional<FinalPowerAndPerfResult>) = 0;
    virtual std::string getDeviceName() const = 0;

    double getK() { return cfg_.k_; } // temporary getter until Eco is reorganised
    void setCustomK(double k) { cfg_.k_ = k; } // temporary setter until Eco is reorganised
    int getNumIterations() { return cfg_.numIterations_; }
    std::string getResultFileName() const { return outResultFileName_; }
    std::string getPowerLogFileName() const { return outPowerFileName_; }
    EcoApi() = default;
    virtual ~EcoApi() = default;
  protected:
    ParamsConfig cfg_; // stores defaults values of params or reads it from config.yaml
    std::string outResultFileName_;
    std::string outPowerFileName_;
};

class Eco : public EcoApi
{
  public:
    void idleSample(int idleTimeS) override;
    FinalPowerAndPerfResult runAppWithSampling(char* const*, int = 1) override;
    FinalPowerAndPerfResult runAppWithSearch(
      char* const*,
      TargetMetric,
      SearchType,
      int = 1) override;
    void plotPowerLog(std::optional<FinalPowerAndPerfResult>) override;
    std::string getDeviceName() const override { return device_->getName(); }

    void staticEnergyProfiler(char* const* argv, BothStream& stream);

    void referenceRunWithoutCaps(char* const*);
    void runAppForEachPowercap(char* const*, BothStream&, Domain = PowerCapDomain::PKG);
    // void storeReferenceRun(FinalPowerAndPerfResult&);

    Eco(std::shared_ptr<IntelDevice>, TriggerType);
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
    std::unique_ptr<Trigger> trigger_;
    std::map<FilterType, DataFilter> smaFilters_;
    FilterType activeFilter_ {FilterType::SMA100};
    DataFilter filter2order_;
    std::shared_ptr<Device> device_;
    CrossDomainQuantity idleAvPow_;
    double pprevSMA_ {0.0}, prevSMA_ {0.0};
    bool optimizationTrigger_ {false};

    DeviceStateAccumulator devStateGlobal_;
    // DeviceStateAccumulator devStateLocal_;
    std::vector<FinalPowerAndPerfResult> fullAppRunResultsContainer_;

    WatchdogStatus defaultWatchdog;
    std::ofstream outPowerFile;
    double defaultPowerLimitInWatts_; // PKG domain power limit

    void storeDataPointToFilters(double);
    double getFilteredPower();
    void modifyWatchdog(WatchdogStatus);
    WatchdogStatus readWatchdog();
    void logPowerToFile();
    std::string generateUniqueResultDir();
    std::vector<int> generateVecOfPowerCaps(Domain = PowerCapDomain::PKG);
    void singleAppRunAndPowerSample(char* const*);
    FinalPowerAndPerfResult multipleAppRunAndPowerSample(char* const*, int);
    PowAndPerfResult checkPowerAndPerformance(int);
    PowAndPerfResult setCapAndMeasure(int, int);
    void justSample(int timeS);
    void reportResult(double = 0.0, double = 0.0);
    void waitPhase(int&, int);
    int testPhase(int&, int&, TargetMetric, SearchType, PowAndPerfResult&, int&, int);
    void execPhase(int, int&, int, PowAndPerfResult&, bool = false);
    void mainAppProcess(char* const*, int&);
    int& adjustHighPowLimit(PowAndPerfResult, int&);
    int linearSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric, int&, int);
    int goldenSectionSearchForBestPowerCap(PowAndPerfResult&, int&, int&, TargetMetric, int&, int);
};


#include "helpers/log.hpp"

class Logger
{
  public:
    Logger(std::string prefix)
    {
        const auto dir = generateUniqueDir(prefix);
        powerFileName_ = dir + "power_log.csv";
        resultFileName_ = dir + "result.csv";
        powerFile_.open(powerFileName_, std::ios::out | std::ios::trunc);
        resultFile_.open(resultFileName_, std::ios::out | std::ios::trunc);
        bout_ = std::make_unique<BothStream>(powerFile_);
    }
    void logPowerLogLine(DeviceStateAccumulator& deviceState, PowAndPerfResult current, const std::optional<PowAndPerfResult> reference = std::nullopt)
    {
        *bout_  << logCurrentGpuResultLine(deviceState.getTimeSinceObjectCreation(), current, reference);
    }
    std::string getPowerFileName() const
    {
        return powerFileName_;
    }
    void flushAndClose() // might be useless
    {
        bout_->flush();
        powerFile_.close();
    }
  private:
    std::string powerFileName_;
    std::ofstream powerFile_;
    std::string resultFileName_;
    std::ofstream resultFile_;
    std::unique_ptr<BothStream> bout_;

    std::string generateUniqueDir(std::string prefix = "")
    {
        std::string dir = prefix + "experiment_" +
            std::to_string(
                std::chrono::system_clock::to_time_t(
                      std::chrono::high_resolution_clock::now()));
        dir += "/";
        const int dir_err = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (-1 == dir_err) {
            printf("Error creating experiment result directory!\n");
            exit(1);
        }
        return dir;
    }
};

using Algorithm = std::function<unsigned(std::shared_ptr<Device>,DeviceStateAccumulator&,TargetMetric,const PowAndPerfResult&,int&,int,int,Logger&)>;

class SearchAlgorithm
{
  public:
    virtual unsigned operator() (
      std::shared_ptr<Device>,
      DeviceStateAccumulator&,
      TargetMetric,
      const PowAndPerfResult&,
      int&,
      int,
      int,
      Logger&) const = 0;

    static PowAndPerfResult sampleAndAccumulatePowAndPerfForGivenPeriod(
      int tuningTimeWindowInMicroSeconds,
      int powerSamplingPeriodInMilliSeconds,
      DeviceStateAccumulator& deviceState,
      Logger& logger)
    {
      auto pauseInMicroSeconds = powerSamplingPeriodInMilliSeconds * 1000;
      usleep(pauseInMicroSeconds);
      deviceState.sample();
      auto resultAccumulator = deviceState.getCurrentPowerAndPerf();
      // std::cout << "\n[INFO] Firstt data point " << resultAccumulator << std::endl;
      while (tuningTimeWindowInMicroSeconds > pauseInMicroSeconds)
      {
          usleep(pauseInMicroSeconds);
          deviceState.sample();
          // logPowerToFile();
          auto tmp = deviceState.getCurrentPowerAndPerf();
          logger.logPowerLogLine(deviceState, tmp);
          // *bout_ << logCurrentGpuResultLine(deviceState.getTimeSinceObjectCreation(), tmp);
          resultAccumulator += tmp;
          // std::cout << "[INFO] Single data point " << tmp << std::endl;
          tuningTimeWindowInMicroSeconds -= pauseInMicroSeconds;
      }
      // std::cout << "[INFO] Finall data point " << resultAccumulator << std::endl;

      return resultAccumulator;
    }
};

class LinearSearchAlgorithm : public SearchAlgorithm
{
  public:
    unsigned operator() (
      std::shared_ptr<Device> device,
      DeviceStateAccumulator& deviceState,
      TargetMetric metric,
      const PowAndPerfResult& reference,
      int& childProcStatus,
      int powerSamplingPeriodInMilliSeconds,
      int tuningTimeWindowInMilliSeconds,
      Logger& logger) const override
    {
      const auto [minLimitInWatts, maxLimitInWatts] = device->getMinMaxLimitInWatts();
      const auto minLimitInMictoWatts = minLimitInWatts * 1e6;
      const auto maxLimitInMictoWatts = maxLimitInWatts * 1e6;
      int STEP = (maxLimitInMictoWatts - minLimitInMictoWatts) / 10;

      auto bestResultSoFar = reference;
      int currentLimitInMicroWatts = maxLimitInMictoWatts;

      while(childProcStatus)
      {
        device->setPowerLimitInMicroWatts(currentLimitInMicroWatts);
        auto&& currentResult = sampleAndAccumulatePowAndPerfForGivenPeriod(
          tuningTimeWindowInMilliSeconds * 1000,
          powerSamplingPeriodInMilliSeconds,
          deviceState,
          logger);
        logger.logPowerLogLine(deviceState, currentResult, reference);
        if (bestResultSoFar.isRightBetter(currentResult, metric))
        {
            bestResultSoFar = std::move(currentResult);
        }
        if (currentLimitInMicroWatts == minLimitInMictoWatts)
        {
            break;
        }
        currentLimitInMicroWatts -= STEP;
        if (currentLimitInMicroWatts < minLimitInMictoWatts)
        {
          currentLimitInMicroWatts = minLimitInMictoWatts;
        }
      }
      return bestResultSoFar.appliedPowerCapInWatts_;
    }
};

class GoldenSectionSearchAlgorithm : public SearchAlgorithm
{
  public:
    unsigned operator() (
      std::shared_ptr<Device> device,
      DeviceStateAccumulator& deviceState,
      TargetMetric metric,
      const PowAndPerfResult& reference,
      int& childProcStatus,
      int powerSamplingPeriodInMilliSeconds,
      int tuningTimeWindowInMilliSeconds,
      Logger& logger) const override
    {
        const auto [minLimitInWatts, maxLimitInWatts] = device->getMinMaxLimitInWatts();
        int EPSILON = (maxLimitInWatts - minLimitInWatts) * 1e6 / 25;

        int a = minLimitInWatts * 1e6; // micro watts
        int b = maxLimitInWatts * 1e6; // micro watts

        int leftCandidateInMicroiWatts = b - int(PHI * (b - a));
        int rightCandidateInMicroWatts = a + int(PHI * (b - a));

        bool measureL = true;
        bool measureR = true;

        PowAndPerfResult tmp = reference;

        while ((b - a) > EPSILON)
        {
          auto fL = tmp;
          if (measureL)
          {
            device->setPowerLimitInMicroWatts(leftCandidateInMicroiWatts);
            fL = sampleAndAccumulatePowAndPerfForGivenPeriod(
              tuningTimeWindowInMilliSeconds * 1000,
              powerSamplingPeriodInMilliSeconds,
              deviceState,
              logger);
            logger.logPowerLogLine(deviceState, fL, reference);
          }
          auto fR = tmp;
          if (measureR)
          {
            device->setPowerLimitInMicroWatts(rightCandidateInMicroWatts);
            fR = sampleAndAccumulatePowAndPerfForGivenPeriod(
              tuningTimeWindowInMilliSeconds * 1000,
              powerSamplingPeriodInMilliSeconds,
              deviceState,
              logger);
            logger.logPowerLogLine(deviceState, fR, reference);
          }

          if (!fL.isRightBetter(fR, metric)) {
            // choose subrange [a, rightCandidateInMilliWatts]
            b = rightCandidateInMicroWatts;
            rightCandidateInMicroWatts = leftCandidateInMicroiWatts;
            tmp = fL;
            measureR = false;
            measureL = true;
            leftCandidateInMicroiWatts = b - int(PHI * (b - a));
          } else {
            // choose subrange [leftCandidateInMilliWatts, b]
            a = leftCandidateInMicroiWatts;
            leftCandidateInMicroiWatts = rightCandidateInMicroWatts;
            tmp = fR;
            measureR = true;
            measureL = false;
            rightCandidateInMicroWatts = a + int(PHI * (b - a));
          }
          logCurrentRangeGSS(a, leftCandidateInMicroiWatts/1000, rightCandidateInMicroWatts/1000, b);
        }
        return (a + b) / 2;
    }
    static constexpr float PHI {(sqrt(5) - 1) / 2}; // this is equal 0.618 and it is reverse of 1.618
  private:
    void logCurrentRangeGSS(int a, int leftCandidateInMilliWatts, int rightCandidateInMilliWatts, int b) const
    {
        std::cout << "#--------------------------------\n"
                  << "# Current GSS range: |"
                  << a << " "
                  << leftCandidateInMilliWatts << " "
                  << rightCandidateInMilliWatts << " "
                  << b << "|\n"
                  << "#--------------------------------\n";
    }
};
