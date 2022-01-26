#include "power_and_perf_result.hpp"

std::ostream& operator<<(std::ostream& os, const PowAndPerfResult& res) {
    if (res.pCap < 0.0) {
        os << "refer.\t";
    } else {
        os << res.pCap << "\t";
    }
    os << res.energy << "\t"
       << res.avP << "\t"
       << res.filteredP << "\t"
       << res.instr << "\t"
       << res.getInstrPerSecond() << "\t"
       << res.getInstrPerJoule() << " \t\t"
       << res.getEnergyTimeProd();
    return os;
}

bool PowAndPerfResult::isRightBetter(PowAndPerfResult& right, TargetMetric mode) {
    if (mode == TargetMetric::MIN_E) {
        return this->getEnergyPerInstr() > right.getEnergyPerInstr();
    } else if (mode == TargetMetric::MIN_E_X_T) {
        return this->getEnergyTimeProd() < right.getEnergyTimeProd();
    } else if (mode == TargetMetric::MIN_M_PLUS) {
        // this is dirty hack as actually Metric Plus is looking for minimum
        // not maximum of this metric.
        return this->myPlusMetric_ > right.myPlusMetric_;
    } else {
        // exception should be thrown as it should never happen to compare with unexpected mode
        return false;
    }
}

double PowAndPerfResult::checkPlusMetric(PowAndPerfResult ref, double k) {
    myPlusMetric_ = (1.0/k) * (ref.getInstrPerSecond()/getInstrPerSecond()) *
                    ((k-1.0) * (avP / ref.avP) + 1.0);
    return myPlusMetric_;
}
