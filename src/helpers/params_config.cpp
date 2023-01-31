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

#include <fstream>
#include <iostream>
#include "params_config.hpp"

ParamsConfig::ParamsConfig() {
    readConfigFile();
    if (!configParamsMap_.empty()) {
        loadParamsFromMap(configParamsMap_);
    }
}

void ParamsConfig::readConfigFile() {
    std::ifstream file (configFileName_.c_str());
    std::string key;
    std::string value;
    if (file.is_open()) {
        std::cout << "\n[INFO] " << configFileName_ << " found, reading params.\n";
        while (file) {
            std::getline (file, key, ':');
            std::getline (file, value, '\n');
            // TODO: fix below hack with proper non integer parameters handling
            // maybe use JSON
            if (key == "k") {
                k_ = atof(value.c_str());
                std::cout << "\tCoefficien for M+ metric is set to "
                        << k_ << ".\n";
            } else {
                configParamsMap_[key] = atoi(value.c_str());
            }
        }
        file.close();
    } else {
        std::cerr << "[Warning] cannot read " << configFileName_
                  << "\n          Check if config file exists.\n";
    }
}

void ParamsConfig::loadParamsFromMap(std::map <std::string, unsigned>& paramsMap) {
    for (auto& element : paramsMap) {
        if (element.first == "msPause") {
            msPause_ = element.second;
            std::cout << "\tSampling time is: " << msPause_ << "\n";
        }
        else if (element.first == "percentStep") {
            percentStep_ = element.second;
            std::cout << "\tPowercaps will decrease by: "
                      << percentStep_ << "%\n";
        }
        else if (element.first == "idleCheckTime") {
            idleCheckTime_ = element.second;
            std::cout << "\tIdle check time set to: "
                      << idleCheckTime_ << "s\n";
        }
        else if (element.first == "numIterations") {
            numIterations_ = element.second;
            std::cout << "\tEach result is an average of "
                      << numIterations_
                      << " test runs.\n";
        }
        else if (element.first == "perfDropStopCondition") {
            perfDropStopCondition_ = element.second;
            std::cout << "\tEnergy profiling for domain will break after "
                      << perfDropStopCondition_
                      << "% drop of performance for tested power limit.\n";
        }
        else if (element.first == "powerSampleOn") {
            powerSampleOn_ = element.second;
            std::cout << "\tEnergy/Power sampling "
                      << (powerSampleOn_ ? "on." : "off") << ".\n";
        }
        else if (element.first == "targetMetric{0-E,1-EDP,2-EDS}") {
            targetMetric_ = element.second;
            std::cout << "\tTarget metric for optimizing application execution: "
                      << (targetMetric_ ? "min(E x t).\n" : "min(E).\n");
        }
        else if (element.first == "msTestPhasePeriod") {
            msTestPhasePeriod_ = element.second;
            usTestPhasePeriod_ = msTestPhasePeriod_ * 1000;
            std::cout << "\tPreparation phase will be examined with "
                      << (double)msTestPhasePeriod_/1000 << "s period.\n";
        }
        else if (element.first == "reducedPowerCapRange") {
            reducedPowerCapRange_ = element.second;
            std::cout << "\tPower caps range is "
                      << (powerSampleOn_ ? "" : "not") << "reduced.\n";
        }
        else if (element.first == "powerLog") {
            isPowerLogOn_ = element.second;
            std::cout << "\tLogging current power to power_log.csv "
                      << (isPowerLogOn_ ? "ENABLED" : "DISABLED") << ".\n";
        }
        else if (element.first == "optimizationDelay") {
            optimizationDelay_ = element.second;
            std::cout << "\tOptimal settings search will start after "
                      << optimizationDelay_ << " seconds.\n";
        }
    }
}