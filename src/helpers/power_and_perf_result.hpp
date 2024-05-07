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

#include "eco_constants.hpp"
#include <iostream>

struct PowAndPerfResult {
    PowAndPerfResult() = default;
    PowAndPerfResult(
        double instructions,
        double timeInSec,
        double powerLimit,
        double energy,
        double avgCorePower,
        double avgMemoryPower,
        double filteredPower) :
        instructionsCount_(instructions),
        periodInSeconds_(timeInSec),
        appliedPowerCapInWatts_(powerLimit),
        energyInJoules_(energy),
        averageCorePowerInWatts_(avgCorePower),
        averageMemoryPowerInWatts_(avgMemoryPower),
        filteredPowerOfLimitedDomainInWatts_(filteredPower)
        {
        }
    double getInstrPerSecond() const { return instructionsCount_/periodInSeconds_; }
    double getInstrPerJoule() const { return instructionsCount_/energyInJoules_; }
    double getEnergyPerInstr() const { return energyInJoules_/instructionsCount_; }
    double getEnergyTimeProd() const { return getInstrPerSecond() * getInstrPerSecond() / averageCorePowerInWatts_; }
    double checkPlusMetric(PowAndPerfResult ref, double k);
    friend std::ostream& operator<<(std::ostream&, const PowAndPerfResult&);
    bool isRightBetter(PowAndPerfResult&, TargetMetric = TargetMetric::MIN_E);
    double instructionsCount_ {0.01};
    double periodInSeconds_ {0.01};
    double appliedPowerCapInWatts_ {0.01};
    double energyInJoules_ {0.01};
    double averageCorePowerInWatts_ {0.01};
    double averageMemoryPowerInWatts_ {0.01};
    double filteredPowerOfLimitedDomainInWatts_ {0.01}; // assume that either Core or Memory is limited
    double myPlusMetric_ {1.0};
};
