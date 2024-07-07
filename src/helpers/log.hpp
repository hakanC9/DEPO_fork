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

#include "power_and_perf_result.hpp"

static inline
std::string logCurrentResultLine(
    PowAndPerfResult& curr,
    PowAndPerfResult& first,
    double k,
    bool noNewLine = false)
{
    std::stringstream sstream;
    if (curr.appliedPowerCapInWatts_ < 0.0) {
        sstream << "refer.\t";
    } else {
        sstream << curr.appliedPowerCapInWatts_ << "\t";
    }
    sstream << std::fixed << std::setprecision(2)
              << curr.energyInJoules_ << "\t"
              << curr.averageCorePowerInWatts_ << "\t"
              << curr.filteredPowerOfLimitedDomainInWatts_ << "\t"
              << std::setprecision(3)
              << curr.getInstrPerSecond() / first.getInstrPerSecond() << "\t"
              << curr.getEnergyPerInstr() / first.getEnergyPerInstr() << "\t"
              // since we seek for min Et and dynamic metric is looking for
              // max of its dynamic version, below for loging purposes the order
              // of division is swaped as it is basically inversion of the relative
              // dynamic metric
              << first.getEnergyTimeProd() / curr.getEnergyTimeProd() << "\t"
              << curr.checkPlusMetric(first, k);
    if (noNewLine) {
        sstream << std::flush;
    } else {
        sstream << "\n";
    }
    return sstream.str();
}

static inline
std::string logCurrentGpuResultLine(
    double timeInMs,
    PowAndPerfResult& curr,
    const std::optional<PowAndPerfResult> reference = std::nullopt,
    double k = 2.0,
    bool noNewLine = false)
{
    std::stringstream sstream;
    sstream << timeInMs
            << std::fixed << std::setprecision(2)
            << "\t\t" << curr.appliedPowerCapInWatts_
            << "\t\t" << curr.averageCorePowerInWatts_
            << "\t\t " << curr.filteredPowerOfLimitedDomainInWatts_
            << "\t\t" << curr.energyInJoules_
            // << "\t\t" << currKernelsCount
            << "\t\t" << curr.instructionsCount_
            << std::fixed << std::setprecision(3)
            << "\t\t" << curr.getInstrPerJoule() * 1000;

    if (reference.has_value())
    {
        double currRelativeENG = curr.getEnergyPerInstr() / reference.value().getEnergyPerInstr();
        // since we seek for min Et and dynamic metric is looking for
        // max of its dynamic version, below for loging purposes the order
        // of division is swaped as it is basically inversion of the relative
        // dynamic metric
        double currRelativeEDP = reference.value().getEnergyTimeProd() / curr.getEnergyTimeProd();
        sstream << "\t\t" << (std::isinf(currRelativeENG) || std::isnan(currRelativeENG) ? 1.0 : currRelativeENG)
                << "\t\t" << (std::isinf(currRelativeEDP) || std::isnan(currRelativeEDP) ? 1.0 : currRelativeEDP)
                << "\t\t" << curr.getInstrPerSecond()
                << "\t\t" << curr.checkPlusMetric(reference.value(), k);
    }
    if (noNewLine) {
        sstream << std::flush;
    } else {
        sstream << "\n";
    }
    return sstream.str();
}


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