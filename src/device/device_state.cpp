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

#include "device_state.hpp"

#include <set>

DeviceStateAccumulator::DeviceStateAccumulator(std::shared_ptr<Device> d) :
    device_(d)
{
    for (auto&& cpuCore : device_->pkgToFirstCoreMap_)
    {
        raplVec_.emplace_back(cpuCore, device_->getAvailablePowerDomains());
        std::cout << "INFO: created RAPL object for core " << cpuCore << " in DeviceStateAccumulator.\n";
    }
}

void DeviceStateAccumulator::resetDevice() {
    for (auto&& rapl : raplVec_)
    {
        rapl.reset();
    }
    device_->resetPerfCounters();
}

void DeviceStateAccumulator::sample() {
    for (auto&& rapl : raplVec_)
    {
        rapl.sample();
    }
}

double DeviceStateAccumulator::getCurrentPower(Domain d) {
    double result = 0.0;
    for (auto&& rapl : raplVec_)
    {
        result += rapl.getCurrentPower()[d];
    }
    return result;
}

double DeviceStateAccumulator::getPerfCounterSinceReset()
{
    return device_->getNumInstructionsSinceReset();
}

double DeviceStateAccumulator::getPkgMaxPower()
{
    return raplVec_.front().pkg_max_power() * raplVec_.size();
    //above may cause problems when vector is empty or when two different CPUs are in one device
}

double DeviceStateAccumulator::getTimeSinceReset() const
{
    std::set<double> timeSet;
    for (auto&& rapl : raplVec_) {
        timeSet.insert(rapl.get_total_time());
    }
    return *timeSet.rbegin(); // set is sorted containter
}

double DeviceStateAccumulator::getEnergySinceReset(Domain d) const
{
    double result = 0.0;
    for (auto&& rapl : raplVec_) {
        result += rapl.getTotalEnergy()[d];
    }
    return result;
}

// Temporary disabled
// ----------------------------------------------------------------------------------------
// std::vector<double> DeviceStateAccumulator::getTotalEnergyVec(Domain d)
// {
//     std::vector<double> result;
//     for (auto&& rapl : raplVec_) {
//         result.push_back(rapl.getTotalEnergy()[d]);
//     }
//     return result;
// }
// ----------------------------------------------------------------------------------------
