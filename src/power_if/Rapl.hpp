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

#include "../device/Device.hpp"
#include "../helpers/eco_constants.hpp"
#include "msr_offsets.hpp"
#include "msr.hpp"

#include <chrono>

#define MAX_PACKAGES	16

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

struct RaplState {
	RaplState() {}
	RaplState(uint64_t pk, uint64_t p0, uint64_t p1, uint64_t d, TimePoint t) :
	    pkg_(pk), pp0_(p0), pp1_(p1), dram_(d), timeSec_(t) {}

	friend RaplState& operator+=(RaplState& , const RaplState&);

	uint64_t pkg_ {0};
	uint64_t pp0_ {0};
	uint64_t pp1_ {0};
	uint64_t dram_ {0};
	TimePoint timeSec_ {std::chrono::high_resolution_clock::now()};
};

class RaplStateSequence {
public:
    void rotateStates();
	void storeNextState(RaplState& rs);
	void reset();
	RaplState getCurrentEnergyIncrement();
	RaplState getPreviousEnergyIncrement();
	double getCurrentTimeIncrement();
	double getPreviousTimeIncrement();
	double getTotalTime(TimePoint startTime);
private:
    uint64_t energyDelta(uint64_t before, uint64_t after);
	double timeDelta(TimePoint&, TimePoint&);
	RaplState next_, current_, previous_;
};

class Rapl {
private:
	int core {0}; // TODO: this should be deleted
	double power_units, energy_units, dram_energy_units, time_units;
	double thermal_spec_power, minimum_power, maximum_power, time_window;

	void read_cpu_info(int, int);
	double power(uint64_t energyIncrement, double time_delta, double units);
	void someCommonCode();

	AvailablePowerDomains availableDomains_;
	int cpuCore_;
	RaplState totalResultSinceLastReset_;
    RaplStateSequence rss_;

public:
	Rapl(int, AvailablePowerDomains);
	~Rapl() {}
	void reset();
	void sample();

	double pkg_current_power();
	double pp0_current_power();
	double pp1_current_power();
	double dram_current_power();

	double pkg_average_power();
	double pp0_average_power();
	double pp1_average_power();
	double dram_average_power();

	double pkg_max_power();

	double pkg_total_energy();
	double pp0_total_energy();
	double pp1_total_energy();
	double dram_total_energy();
	EnergyCrossDomains getTotalEnergy();
	PowerCrossDomains getAveragePower();
	PowerCrossDomains getCurrentPower();

	double total_time();
	double current_time();
};
