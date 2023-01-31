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
    PowAndPerfResult() {}
    PowAndPerfResult(double i, double per, double pc, double e, double avp, double fp)
        : instr(i), period(per), pCap(pc), energy(e), avP(avp), filteredP(fp) {}
    double instr {0.01}, period {0.01}, pCap {0.01}, energy {0.01}, avP {0.01}, filteredP {0.01};
    double myPlusMetric_ {1.0};
    double getInstrPerSecond() const { return instr/period; }
    double getInstrPerJoule() const { return instr/energy; }
    double getEnergyPerInstr() const { return energy/instr; }
    double getEnergyTimeProd() const { return getInstrPerSecond() * getInstrPerSecond() / avP; }
    double checkPlusMetric(PowAndPerfResult ref, double k);
    friend std::ostream& operator<<(std::ostream&, const PowAndPerfResult&);
    bool isRightBetter(PowAndPerfResult&, TargetMetric = TargetMetric::MIN_E);

};