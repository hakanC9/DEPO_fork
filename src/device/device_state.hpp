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

class DeviceState {
public:
    DeviceState(std::shared_ptr<Device>);
    ~DeviceState() {}
    double getTotalAveragePower(Domain d);
    double getTotalEnergy(Domain d);
    std::vector<double> getTotalEnergyVec(Domain d);
    double getTotalTime();
    void sample();
    void resetDevice();
    double getPkgMaxPower();
    double getCurrentPower(Domain d);

private:
    std::shared_ptr<Device> device_;
    std::vector<Rapl> raplVec_;
};
