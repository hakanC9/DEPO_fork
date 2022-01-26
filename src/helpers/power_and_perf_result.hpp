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