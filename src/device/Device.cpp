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

#include "Device.hpp"
#include "../../common_const_intel.hpp"

#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>

#define MAX_CPUS		1024

AvailablePowerDomains::AvailablePowerDomains (bool p0, bool p1, bool d, bool ps, bool du) :
    pp0_(p0), pp1_(p1), dram_(d), psys_(ps), fixedDramUnits_(du) {
}

Device::Device() {
    detectCPU();
    detectPackages();
    detectPowerCapsAvailability();
}

int Device::getNumPackages() {
    return totalPackages_;
}

int Device::getModel() {
    return model_;
}

std::string Device::getCPUname() {
    return mapCpuFamilyName(model_);
}

AvailablePowerDomains Device::getAvailablePowerDomains() {
    return devicePowerProfile_;
}

void Device::detectCPU() {

	FILE *fff;

	int family;
	char buffer[BUFSIZ];
    char *result;
	char vendor[BUFSIZ];

	fff = fopen("/proc/cpuinfo", "r");
// TODO: rework above check and below loop
	while(1) {
		result=fgets(buffer,BUFSIZ,fff);
		if (result==NULL) break;

		if (!strncmp(result,"vendor_id",8)) {
			sscanf(result,"%*s%*s%s", vendor);

			if (strncmp(vendor, "GenuineIntel", 12)) {
				printf("%s not an Intel chip\n", vendor);
			}
		}
		if (!strncmp(result,"cpu family",10)) {
			sscanf(result,"%*s%*s%*s%d", &family);
			if (family!=6) {
				printf("Wrong CPU family %d\n",family);
			}
		}
		if (!strncmp(result,"model",5)) {
			sscanf(result,"%*s%*s%d", &model_);
		}
	}
	fclose(fff);

	std::fstream cpuParams;
	std::string filename = "cpu_params.txt";
	cpuParams.open(filename.c_str());
	if (cpuParams.is_open()) {
		cpuParams.close();
	}
	else {
		std::ofstream outStream (filename.c_str(), std::ios::out | std::ios::trunc);
		outStream << "vendor: " << vendor << "\n";
		outStream << "Family:  " << family << "\n";
		outStream << "Family name:  " << mapCpuFamilyName(model_) << "\n";
		outStream.close();
	}
	if (family == 6) {
		std::cout << "Detected suported CPU\nModel: " << mapCpuFamilyName(model_) << "\n";
	}
}

void Device::detectPackages() {

	FILE *fff;
	int package;
	std::stringstream filenameStream;

	printf("\t");
	for(int i = 0; i < MAX_CPUS; i++) {
		filenameStream << "/sys/devices/system/cpu/cpu" << i << "/topology/physical_package_id";
		fff = fopen(filenameStream.str().c_str(), "r");
		if (fff == NULL) break;
		fscanf(fff, "%d", &package);
		printf("%d (%d)", i, package);
		if (i % 8 == 7) printf("\n\t"); else printf(", ");
		fclose(fff);

		if (pkgToFirstCoreMap_.size() <= package) {
			totalPackages_++;
			pkgToFirstCoreMap_.push_back(i);
		}
		filenameStream.str("");
		totalCores_++;
	}

	printf("\n");

	printf("\tDetected %d cores in %d packages\n\n", totalCores_, totalPackages_);
}

std::string Device::mapCpuFamilyName(int& model){
	switch(model) {
		case CPU_SANDYBRIDGE:
			return std::string("Sandybridge");
		case CPU_SANDYBRIDGE_EP:
			return std::string("Sandybridge-EP");
		case CPU_IVYBRIDGE:
			return std::string("Ivybridge");
		case CPU_IVYBRIDGE_EP:
			return std::string("Ivybridge-EP");
		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
			return std::string("Haswell");
		case CPU_HASWELL_EP:
			return std::string("Haswell-EP");
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
			return std::string("Broadwell");
		case CPU_BROADWELL_EP:
			return std::string("Broadwell-EP");
		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
			return std::string("Skylake");
		case CPU_SKYLAKE_X:
			return std::string("Skylake-X");
		case CPU_KABYLAKE:
		case CPU_KABYLAKE_MOBILE:
			return std::string("Kaby Lake");
		case CPU_KNIGHTS_LANDING:
			return std::string("Knight's Landing");
		case CPU_KNIGHTS_MILL:
			return std::string("Knight's Mill");
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			return std::string("Atom");
		default:
			model=-1;
            return std::string("Unsupported model");
	}
}

void Device::detectPowerCapsAvailability() {
	if (model_ < 0) {
		printf("\tUnsupported CPU model %d\n", model_);
	} //TODO - throw exception and handle it in Eco/Rapl

	switch(model_) {

		case CPU_SANDYBRIDGE_EP:
		case CPU_IVYBRIDGE_EP:
			devicePowerProfile_ = AvailablePowerDomains(true, false, true, false, false);
			break;

		case CPU_HASWELL_EP:
		case CPU_BROADWELL_EP: // according to real system (PC Xeon E5) there is no PP0
		case CPU_SKYLAKE_X: // TODO: To be checked on Skylake X if possible.
			devicePowerProfile_ = AvailablePowerDomains(false, false, true, false, true);
			break;

		case CPU_KNIGHTS_LANDING:
		case CPU_KNIGHTS_MILL:
			devicePowerProfile_ = AvailablePowerDomains(false, false, true, false, true);
			break;

		case CPU_SANDYBRIDGE:
		case CPU_IVYBRIDGE:
			devicePowerProfile_ = AvailablePowerDomains(true, true, false, false, false);
			break;

		case CPU_HASWELL:
		case CPU_HASWELL_ULT:
		case CPU_HASWELL_GT3E:
		case CPU_BROADWELL:
		case CPU_BROADWELL_GT3E:
		case CPU_ATOM_GOLDMONT:
		case CPU_ATOM_GEMINI_LAKE:
		case CPU_ATOM_DENVERTON:
			devicePowerProfile_ = AvailablePowerDomains(true, true, true, false, false);
			break;

		case CPU_SKYLAKE:
		case CPU_SKYLAKE_HS:
		case CPU_KABYLAKE: // TODO: confirm below settings with some doc. Actually des19 (KabyLake) has PKG/PP0/DRAM
						   //       and does not support PP1. No info about PSys either.
		case CPU_KABYLAKE_MOBILE:
			devicePowerProfile_ = AvailablePowerDomains(true, false, true, true, false);
			break;
	}
}