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
#include "device/device_state.hpp"
#include "device/Device.hpp"

#include <cuda.h>
#include <nvml.h>


/**
 * This class represents single CUDA device pointed by the deviceID
 * during object construction. However, it stores all the device handles
 * and it is able to read power or write power limit to any existing
 * in the system CUDA device.
 * By design at the moment the get/set power methods use only deviceID_
 * which is a member of the object.
 *
 * FUTURE WORK:
 * In the future this may change when, e.g., DEPO or StEP would consider
 * multi-gpu support.
*/
class CudaDevice : public Device
{
  public:
    CudaDevice(int devID = 0);

    double getPowerLimitInWatts() const override;
    void setPowerLimitInMicroWatts(unsigned long limitInMicroW) override;
    std::string getName() const override;

    std::pair<unsigned, unsigned> getMinMaxLimitInWatts() const override;
    void reset() override;
    double getCurrentPowerInWatts() const override;
    unsigned long long int getPerfCounter() const;
    void triggerPowerApiSample() override {}; // empty method since, NVIDIA GPU does not need to explicit trigger API sampling



  private:
    void initDeviceHandles();
    nvmlReturn_t nvResult_;
    unsigned int deviceCount_ {0};
    int deviceID_;
    std::vector<nvmlDevice_t> deviceHandles_;
};


// class GpuDeviceState
// {
//   public:
//     GpuDeviceState(std::shared_ptr<CudaDevice>& device);

//     GpuDeviceState& sample();
//     void resetState();


//     /*
//       getCurrentPowerAndPerf - needed mostly (only?) for logging purposes

//       returns the PowerAndPerfResult struct with the data based on the difference
//       between next and current state. Such data is used for power log.
//     */
//     PowAndPerfResult getCurrentPowerAndPerf(int deviceID) const;


//     /*
//       getEnergySinceReset - is used for the final evaluation of energy consumed

//       returns the integrate of energy sampled since last Accumulator reset.
//     */
//     double getEnergySinceReset() const;


//     /*
//       getTimeSinceReset - is used for the final evaluation of time spent on computations

//       returns the time difference between now and last Accumulator reset.
//     */
//     template <class Resolution = std::chrono::milliseconds>
//     double getTimeSinceReset() const
//     {
//         return std::chrono::duration_cast<Resolution>(
//                     std::chrono::high_resolution_clock::now()  - timeOfLastReset_).count();
//     }


//     /*
//       getTimeSinceObjectCreation - used only for logging purposes.

//       returns the absolute time since the creation of the device state accumulator. Needed
//       for logging data used for power log creation.
//     */
//     template <class Resolution = std::chrono::milliseconds>
//     double getTimeSinceObjectCreation() const
//     {
//         return std::chrono::duration_cast<Resolution>(
//                     std::chrono::high_resolution_clock::now()  - absoluteStartTime_).count();
//     }

//   private:
//     TimePoint absoluteStartTime_;
//     TimePoint timeOfLastReset_;
//     std::shared_ptr<CudaDevice> gpu_;
//     PowerAndPerfState prev_, curr_, next_;
//     double totalEnergySinceReset_ {0.0};
// };

class GpuEco : public EcoApi
{
  public:
    GpuEco(int deviceID = 0);
    virtual ~GpuEco();

    void idleSample(int sleepPeriodInMs) override;
    FinalPowerAndPerfResult runAppWithSampling(char* const* argv, int argc) override;
    FinalPowerAndPerfResult runAppWithSearch(
        char* const* argv,
        TargetMetric metric,
        SearchType searchType,
        int argc) override;
    void plotPowerLog() override;
    std::string getDeviceName() const override { return gpu_->getName(); }

    void staticEnergyProfiler(char* const* argv, int argc, BothStream& stream);

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
    std::unique_ptr<DeviceStateAccumulator> deviceState_;
    unsigned minPowerLimit_, maxPowerLimit_;
    double defaultPowerLimitInWatts_;
    int deviceID_;
    std::ofstream outPowerFile_;
    std::unique_ptr<BothStream> bout_;
  public:
    std::string outPowerFileName_;
};
