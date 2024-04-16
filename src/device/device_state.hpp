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

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

class DeviceStateAccumulator {
public:
    DeviceStateAccumulator(std::shared_ptr<IntelDevice>);
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

    /*
      getTimeSinceObjectCreation - used only for logging purposes.

      returns the absolute time since the creation of the device state accumulator. Needed
      for logging data used for power log creation.
    */
    template <class Resolution = std::chrono::milliseconds>
    double getTimeSinceObjectCreation() const
    {
        return std::chrono::duration_cast<Resolution>(
                    std::chrono::high_resolution_clock::now()  - absoluteStartTime_).count();
    }

    // Temporary disabled
    // ----------------------------------------------------------------------------------------
    // std::vector<double> getTotalEnergyVec(Domain d);

    void sample();
    void resetDevice();
    double getCurrentPower(Domain d);
    double getPerfCounterSinceReset();

private:
    TimePoint absoluteStartTime_;
    std::shared_ptr<IntelDevice> device_;
    std::vector<Rapl> raplVec_;
};
