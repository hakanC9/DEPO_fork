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