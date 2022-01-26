#include "msr.hpp"
#include "msr_offsets.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cmath>

MSR::MSR(int core) {
    openMSR(core);
}

MSR::~MSR() {
    if (fileDescriptor_ != UNDEFINED_FD) {
        close(fileDescriptor_);
    }
}

void MSR::openMSR(int core) {
    std::stringstream filenameStream;
    filenameStream << "/dev/cpu/" << core << "/msr";
    fileDescriptor_ = open(filenameStream.str().c_str(), O_RDWR);
    if (fileDescriptor_ < 0) {
        if ( errno == ENXIO) {
            fprintf(stderr, "rdmsr: No CPU %d\n", core);
            exit(2);
        } else if ( errno == EIO) {
            fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
            exit(3);
        } else {
            perror("rdmsr:open");
            fprintf(stderr, "Trying to open %s\n", filenameStream.str().c_str());
            exit(127);
        }
    }
}

uint64_t MSR::readMSR(int offset) {
    uint64_t data;
    if (pread(fileDescriptor_, &data, sizeof(data), offset) != sizeof(data)) {
        perror("read_msr():pread");
        exit(127);
    }
    return data;
}

void MSR::writeMSR(int offset, uint64_t value) {
    if (pwrite(fileDescriptor_, (const void *)&value, sizeof(uint64_t), offset) != sizeof(value)) {
        perror("write_msr():pwrite");
        exit(127);
    }
}

uint64_t MSR::getEnergyStatus(Domain domain) {
    constexpr uint32_t MAX_INT = ~((uint32_t) 0);

    switch (domain) {
        case Domain::PKG :
            return readMSR(MSR_PKG_ENERGY_STATUS) & MAX_INT;
            break;
        case Domain::PP0 :
            return readMSR(MSR_PP0_ENERGY_STATUS) & MAX_INT;
            break;
        case Domain::PP1 :
            return readMSR(MSR_PP1_ENERGY_STATUS) & MAX_INT;
            break;
        case Domain::DRAM :
            return readMSR(MSR_DRAM_ENERGY_STATUS) & MAX_INT;
            break;
    }
}

double MSR::getUnits(Quantity q) {
    uint64_t rawValue = readMSR(MSR_RAPL_POWER_UNIT);
	switch (q) {
		case Quantity::Energy:
		    return pow(0.5, (double) ((rawValue >> 8) & 0x1f));
			break;
		case Quantity::Power:
		    return pow(0.5, (double) (rawValue & 0xf));
			break;
		case Quantity::Time:
		    return pow(0.5, (double) ((rawValue >> 16) & 0xf));
			break;
	}
}

double MSR::getFixedDramUnitsValue() {
	return pow(0.5,(double)16);
}

PowerInfo MSR::getPowerInfoForPKG() {
    uint64_t rawValue = readMSR(MSR_PKG_POWER_INFO);
	PowerInfo result;
	auto powerUnits = getUnits(Quantity::Power);
    result.thermalDesignPower = powerUnits * ((double)(rawValue & 0x7fff));
    result.minPower = powerUnits * ((double)((rawValue >> 16) & 0x7fff));
    result.maxPower = powerUnits * ((double)((rawValue >> 32) & 0x7fff));
    result.maxTimeWindow = getUnits(Quantity::Time) * ((double)((rawValue >> 48) & 0x7fff));
	return result;
}

int MSR::getOffsetForPowerLimit(Domain domain) {
	int offset;
	switch (domain) {
		case Domain::PKG :
		    offset = MSR_PKG_RAPL_POWER_LIMIT;
			break;
		case Domain::PP0 :
		    offset = MSR_PP0_POWER_LIMIT;
			break;
		case Domain::PP1 :
		    offset = MSR_PP1_POWER_LIMIT;
			break;
		case Domain::DRAM :
		    offset = MSR_DRAM_POWER_LIMIT;
			break;
	}
    return offset;
}

void MSR::enableClamping(Domain domain) {
	auto offset = getOffsetForPowerLimit(domain);
    uint64_t rawValue = readMSR(offset);
	writeMSR(offset, (rawValue | (0x1 << 16)));
}

void MSR::enablePowerCapping(Domain domain) {
	auto offset = getOffsetForPowerLimit(domain);
    uint64_t rawValue = readMSR(offset);
	writeMSR(offset, (rawValue | (0x1 << 15)));
}

void MSR::disableClamping(Domain domain) {
	auto offset = getOffsetForPowerLimit(domain);
    uint64_t rawValue = readMSR(offset);
	writeMSR(offset, (rawValue & ~(0x1 << 16)));
}

void MSR::disablePowerCapping(Domain domain) {
	auto offset = getOffsetForPowerLimit(domain);
    uint64_t rawValue = readMSR(offset);
	writeMSR(offset, (rawValue & ~(0x1 << 15)));
}

bool MSR::checkLockedByBIOS() {
	uint64_t rawValue = readMSR(getOffsetForPowerLimit(Domain::PKG));
	bool result = (rawValue >> 63) == 1;
    if (result) {
        std::cerr << "WARNING!\tPackage power limits are locked by BIOS\n";
	}
	return result;
}