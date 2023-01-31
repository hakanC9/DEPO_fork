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

#include <string>
#include <vector>
#include <memory>

struct AvailablePowerDomains {
    AvailablePowerDomains() {}
    AvailablePowerDomains(bool, bool, bool, bool, bool);
    ~AvailablePowerDomains() {}

    bool pp0_{false}, pp1_{false}, dram_{false}, psys_{false}, fixedDramUnits_{false};
};

class Device {
public:
    Device();
    ~Device() {}
    int getNumPackages();
    int getModel();
    std::string getCPUname();
    AvailablePowerDomains getAvailablePowerDomains();
    std::vector<int> pkgToFirstCoreMap_; // TODO getter for this, preferably delete

private:
    void detectCPU();
    void detectPackages();
    void detectPowerCapsAvailability();
    std::string mapCpuFamilyName(int& model);

    int totalPackages_ {0};
    int totalCores_ {0};
    int model_ {-1};
    AvailablePowerDomains devicePowerProfile_;
};
