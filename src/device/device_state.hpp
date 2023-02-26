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

#include "Device.hpp"
#include "../power_if/Rapl.hpp"

class DeviceStateAccumulator {
public:
    DeviceStateAccumulator(std::shared_ptr<Device>);
    ~DeviceStateAccumulator() {}

    /*
      getEnergySinceReset - is used for the final evaluation of energy consumed

      returns the integrate of energy sampled since last Accumulator reset.
    */
    double getEnergySinceReset(Domain d) const;

    /*
      getTimeSinceReset - is used for the final evaluation of time spent on computations

      returns the time difference between now and last Accumulator reset.
    */
    double getTimeSinceReset() const;

    // Temporary disabled
    // ----------------------------------------------------------------------------------------
    // std::vector<double> getTotalEnergyVec(Domain d);

    void sample();
    void resetDevice();
    double getPkgMaxPower();
    double getCurrentPower(Domain d);
    double getPerfCounterSinceReset();

private:
    std::shared_ptr<Device> device_;
    std::vector<Rapl> raplVec_;
};
