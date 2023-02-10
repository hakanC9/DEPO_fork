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

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include "Eco.hpp"
#include "plot_builder.hpp"
#include "helpers/params_config.hpp"
#include "helpers/both_stream.hpp"
#include "helpers/power_and_perf_result.hpp"
#include "helpers/log.hpp"

#include <cuda.h>
#include <nvml.h>

static inline
void logCurrentRangeGSS(int a, int leftCandidate, int rightCandidate, int b)
{
    std::cout << "#--------------------------------\n"
              << "# Current GSS range: |"
              << a << " "
              << leftCandidate << " "
              << rightCandidate << " "
              << b << "|\n"
              << "#--------------------------------\n";
}

static inline
std::string parseArgvCommandToString(int argc, char** argv)
{
    std::stringstream str;
    str << "# ";
    for (int i=1; i < argc; i++) {
        str << argv[i] << " ";
    }
    str << "\n";
    return str.str();
}

static inline
std::string generateUniqueDir()
{
    std::string dir = "gpu_experiment_" +
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

static inline
int long long readValueFromFile (std::string fileName)
{
    std::ifstream file (fileName.c_str());
    std::string line;
    long long int value = -1;
    if (file.is_open())
    {
        while ( getline (file, line) )
        {
            value = atoi(line.c_str());
        }
        file.close();
    }
    else
    {
        std::cerr << "cannot read the value from file: " << fileName << "\n"
                  << "file not open\n";
    }
    return value;
}

class CudaDevice // this class should be named "cuda device container or sth like that as it stores all the devices"
{
  public:
    CudaDevice(int devID=0);

    int getPowerLimit(unsigned deviceID);
    void setPowerLimit(unsigned deviceID, int limitInWatts);
    std::pair<unsigned, unsigned> getMinMaxLimitInWatts(unsigned deviceID);
    void resetKernelCounterRegister();
    double getCurrentPowerInWattsForDeviceID(); // this method shall have the input parameter "deviceID" back

  private:
    void initDeviceHandles();
    nvmlReturn_t nvResult_;
    unsigned int deviceCount_ {0};
    int deviceID_;
    std::vector<nvmlDevice_t> deviceHandles_;
};

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

struct NvidiaState
{
    NvidiaState() = delete;
    NvidiaState(double pow, unsigned long long ker, TimePoint t) :
        power_(pow), kernelsCount_(ker), time_(t)
    {
    }
    double power_;
    unsigned long long kernelsCount_;
    TimePoint time_;
};

class GpuDeviceState
{
  public:
    GpuDeviceState(std::shared_ptr<CudaDevice>& device);

    GpuDeviceState& sample();
    PowAndPerfResult getCurrentPowerAndPerf(int deviceID) const;
    double getEnergySinceReset() const;
    void resetState();

    template <class Resolution = std::chrono::milliseconds>
    double getTimeSinceReset() const
    {
        return std::chrono::duration_cast<Resolution>(
                    std::chrono::high_resolution_clock::now()  - timeOfLastReset_).count();
    }

    template <class Resolution = std::chrono::milliseconds>
    double getAbsoluteTimeSinceStart() const
    {
        return std::chrono::duration_cast<Resolution>(
                    std::chrono::high_resolution_clock::now()  - absoluteStartTime_).count();
    }

  private:
    TimePoint absoluteStartTime_;
    TimePoint timeOfLastReset_;
    std::shared_ptr<CudaDevice> gpu_;
    NvidiaState prev_, curr_, next_;
    double totalEnergySinceReset_ {0.0};
};

class GpuEco : public EcoApi
{
  public:
    GpuEco(int deviceID = 0);
    virtual ~GpuEco();

    void idleSample(int milliseconds) override;

    FinalPowerAndPerfResult runAppWithSampling(char* const* argv, int argc) override;
    void staticEnergyProfiler(char* const* argv, int argc, BothStream& stream);
    FinalPowerAndPerfResult runAppWithSearch(
        char* const* argv,
        TargetMetric metric,
        SearchType searchType,
        int argc) override;

    void plotPowerLog() override;
    void waitForGpuComputeActivity(int& status, int samplingPeriodInMilliSec);

    PowAndPerfResult getReferenceResult(const int referenceSampleTimeInMilliSec);

    int runTunningPhaseLS(
        int& status,
        int samplingPeriodInMilliSec,
        const PowAndPerfResult& referenceResult,
        TargetMetric metric);

    int runTunningPhaseGSS(
        int& status,
        int samplingPeriodInMilliSec,
        const PowAndPerfResult& referenceResult,
        TargetMetric metric);

    void executeWithPowercap(
        int& status,
        unsigned powercapInWatts,
        int samplingPeriodInMilliSec,
        int childPID,
        const PowAndPerfResult& referenceResult);

  private:
    std::shared_ptr<CudaDevice> gpu_;
    std::unique_ptr<GpuDeviceState> deviceState_;
    unsigned minPowerLimit_, maxPowerLimit_, defaultPowerLimit_;
    int deviceID_;
    std::ofstream outPowerFile_;
    std::unique_ptr<BothStream> bout_;
  public:
    std::string outPowerFileName_;
};
