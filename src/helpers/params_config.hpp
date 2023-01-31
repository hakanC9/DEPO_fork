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
#include <map>

class ParamsConfig {
public:
    ParamsConfig();
    const std::string configFileName_ {"params.conf"};
	int msPause_ {5}; // sampling time
    int percentStep_ {5};
    int idleCheckTime_ {5};
    int numIterations_ {5};
    int perfDropStopCondition_ {100};
    int powerSampleOn_ {1};
    int targetMetric_ {0}; // 0 - Energy by default, 1 - EDP, 2 - EDS
    int msTestPhasePeriod_ {1000};
    int usTestPhasePeriod_ {1000 * 1000};
    int reducedPowerCapRange_ {0};
    int optimizationDelay_ {0}; // seconds
    int isPowerLogOn_ {1};
    double k_ {1.0};
    std::map <std::string, unsigned> configParamsMap_;
private:
    void readConfigFile();
    void loadParamsFromMap(std::map <std::string, unsigned>&);
};