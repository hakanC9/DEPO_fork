/*
   Copyright 2022-2024, Adam Krzywaniak.

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

#include <iostream>
#include <iomanip>
#include "Eco.hpp"
#include <sys/wait.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include "plot_builder.hpp"
#include "helpers/log.hpp"

static constexpr char FLUSH_AND_RETURN[] = "\r                                                                                     \r";

Eco::Eco(std::shared_ptr<IntelDevice> d) :
    filter2order_(100), device_(d), devStateGlobal_(d), devStateLocal_(d)
{

    // include in DirBulder
    auto dir = generateUniqueResultDir();
    outResultFileName_ = dir + "/result.csv";
    if (cfg_.isPowerLogOn_)
    {
        outPowerFileName_ = dir + "/power_log.csv";
        outPowerFile.open(outPowerFileName_, std::ios::out | std::ios::trunc);
        // below should not be hardcoded but depending on type of series of data
        outPowerFile << "#time[ms]\tP_cap[W]\tPKG[W]\tSMA50\tSMA100\tSMA200\tDRAM[W]\tabsolute time (for sync with another tests)\n";
    }
    defaultWatchdog = readWatchdog();
    if (defaultWatchdog == WatchdogStatus::ENABLED)
    {
        modifyWatchdog(WatchdogStatus::DISABLED);
    }

    smaFilters_.emplace(FilterType::SMA50, 50);
    smaFilters_.emplace(FilterType::SMA100, 100);
    smaFilters_.emplace(FilterType::SMA200, 200);
    smaFilters_.emplace(FilterType::SMA_1_S, 1000 / cfg_.msPause_);
    smaFilters_.emplace(FilterType::SMA_2_S, 2000 / cfg_.msPause_);
    smaFilters_.emplace(FilterType::SMA_5_S, 5000 / cfg_.msPause_);
    smaFilters_.emplace(FilterType::SMA_10_S, 10000 / cfg_.msPause_);
    smaFilters_.emplace(FilterType::SMA_20_S, 20000 / cfg_.msPause_);
    checkIdlePowerConsumption();
    defaultPowerLimitInWatts_ = device_->getPowerLimitInWatts();
}

Eco::~Eco() {
    // device_->restoreDefaults();
    device_->setPowerLimitInMicroWatts(10e6 * defaultPowerLimitInWatts_);
    modifyWatchdog(defaultWatchdog);
    outPowerFile.close();
}


std::string Eco::generateUniqueResultDir() {
    std::string dir = "cpu_experiment_" + std::to_string(
            std::chrono::system_clock::to_time_t(
                  std::chrono::high_resolution_clock::now()));

    const int dir_err = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == dir_err) {
        printf("Error creating experiment result directory!\n");
        exit(1);
    }
    return dir;
}

static inline int readLimitFromFile (std::string fileName) {
    std::ifstream limitFile (fileName.c_str());
    std::string line;
    int limit = -1;
    if (limitFile.is_open()){
        while ( getline (limitFile, line) ){
            limit = atoi(line.c_str());
        }
        limitFile.close();
    } else {
        std::cerr << "cannot read the limit file: " << fileName << "\n"
                  << "file not open\n";
    }
    return limit;
}

static inline void writeLimitToFile (std::string fileName, int limit) {
    std::ofstream outfile (fileName.c_str(), std::ios::out | std::ios::trunc);
    if (outfile.is_open()){
        outfile << limit;
    } else {
        std::cerr << "cannot write the limit to file " << fileName << "\n"
                  << "file not open\n";
    }
    outfile.close();
}

std::vector<int> Eco::generateVecOfPowerCaps(Domain dom) {
    std::vector<int> powerLimitsVec;

    int lowPowLimit_uW = (int)(idleAvPow_[dom] * 1000000 + 0.5); // add 0.5 to round-up double while casting to int
    int highPowLimit_uW = device_->getMinMaxLimitInWatts().second * 1000000;//(int)(highPowW * 1000000 + 0.5); // same

    int step = ((highPowLimit_uW - lowPowLimit_uW)/ 100) * cfg_.percentStep_;
    for (int limit_uW = highPowLimit_uW; limit_uW > lowPowLimit_uW; limit_uW -= step) {
        powerLimitsVec.push_back(limit_uW);
    }
    std::cout << "Vector generated, length: " << powerLimitsVec.size() << "\n";
    return powerLimitsVec;
}

using MS = std::chrono::milliseconds;

void Eco::storeDataPointToFilters(double currPower) {
    for (auto&& filter : smaFilters_) {
        filter.second.storeDataPoint(currPower);
    }
}

void Eco::raplSample() {
    devStateGlobal_.sample();
    pprevSMA_ = prevSMA_;
    filter2order_.storeDataPoint(smaFilters_.at(FilterType::SMA_20_S).getSMA());
    auto currPower = devStateGlobal_.getCurrentPower(Domain::PKG);
    storeDataPointToFilters(currPower);
    auto currSMA = smaFilters_.at(FilterType::SMA_20_S).getSMA();
    optimizationTrigger_ = filter2order_.getCleanedRelativeError() < 0.03;
    if (outPowerFile.is_open()) {
        outPowerFile << std::fixed << std::setprecision(4)
                     << devStateGlobal_.getTimeSinceObjectCreation<MS>()
                     << "\t" << device_->getPowerLimitInWatts() << "\t"
                     << currPower << "\t"
                     << currSMA << "\t"
                     << filter2order_.getSMA() << "\t"
                     << filter2order_.getCleanedRelativeError() << "\t"
                     << devStateGlobal_.getCurrentPower(Domain::DRAM) << "\t"
                     << std::chrono::duration_cast<MS>(
                         std::chrono::high_resolution_clock::now().time_since_epoch()).count()
                     << std::endl;
    }
}

void Eco::checkIdlePowerConsumption() {
    std::cout << "\nChecking idle average power consumption for " << cfg_.idleCheckTime_ << "s.\n";
    for (auto i = 0; i < cfg_.idleCheckTime_ * 1000; i += cfg_.msPause_) {
        if (!(i%1000)) std::cout << "." << std::flush;
        raplSample();
        usleep(cfg_.msPause_ * 1000);
    }
    std::cout << FLUSH_AND_RETURN;
    // TODO: assign structure object, not each element separately
    double totalTimeInSeconds = devStateGlobal_.getTimeSinceReset<std::chrono::seconds>();

    idleAvPow_[Domain::PKG] = devStateGlobal_.getEnergySinceReset(Domain::PKG) / totalTimeInSeconds;
    idleAvPow_[Domain::PP0] = devStateGlobal_.getEnergySinceReset(Domain::PP0) / totalTimeInSeconds;
    idleAvPow_[Domain::PP1] = devStateGlobal_.getEnergySinceReset(Domain::PP1) / totalTimeInSeconds;
    idleAvPow_[Domain::DRAM] = devStateGlobal_.getEnergySinceReset(Domain::DRAM) / totalTimeInSeconds;
    std::cout << std::fixed << std::setprecision(3)
              << "\nIdle average power consumption:\n"
              << "\t PKG: " << idleAvPow_[Domain::PKG] << " W\n"
              << "\t PP0: " << idleAvPow_[Domain::PP0] << " W\n"
              << "\t PP1: " << idleAvPow_[Domain::PP1] << " W\n"
              << "\tDRAM: " << idleAvPow_[Domain::DRAM] << " W\n";
}

void Eco::singleAppRunAndPowerSample(char* const* argv) {
    devStateGlobal_.resetState();

    int fd = open("EP_stdout.txt", O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }
    pid_t childProcId = fork();
    if (childProcId >= 0) { //fork successful
        if (childProcId == 0) { // child process
            mainAppProcess(argv, fd);
        }
        else { // parent process
            int status = 1;
            waitpid(childProcId, &status, WNOHANG);
            if(cfg_.powerSampleOn_) {
                while (status) {
                    usleep(cfg_.msPause_ * 1000);
                    raplSample();
                    waitpid(childProcId, &status, WNOHANG);
                }
            }
        }
    } else {
        std::cerr << "fork failed" << std::endl;
        // TODO: handle errors
        // return 1;
    }
}

void Eco::idleSample(int idleTimeS) {
    justSample(idleTimeS);
}

void Eco::justSample(int timeS) {
    int delayUS = timeS * 1000 * 1000;
    for (int i = 0; i <= delayUS; i+= cfg_.usTestPhasePeriod_) {
        checkPowerAndPerformance(cfg_.usTestPhasePeriod_);
    }
}

void Eco::localPowerSample(int usPeriod) {
    auto pause = cfg_.msPause_ * 1000;
    while (usPeriod > 0){
        usleep(pause);
        raplSample();
        devStateLocal_.sample();
        usPeriod -= pause;
    }
}

PowAndPerfResult Eco::checkPowerAndPerformance(int usPeriod) {
    devStateLocal_.resetState();
    localPowerSample(usPeriod);
    double timeInSeconds = (double)usPeriod / 10e6;
    double energyInJoules = devStateLocal_.getEnergySinceReset(Domain::PKG);

    return PowAndPerfResult(devStateLocal_.getPerfCounterSinceReset(),
                            timeInSeconds,
                            device_->getPowerLimitInWatts(),
                            energyInJoules,
                            energyInJoules / timeInSeconds, // Joules / seconds = Watts
                            0.0, // DRAM power - TBD
                            getFilteredPower());
}

double Eco::getFilteredPower() {
    return smaFilters_.at(activeFilter_).getSMA();
}

PowAndPerfResult Eco::setCapAndMeasure(int cap,
                                       int usPeriod) {
    device_->setPowerLimitInMicroWatts(cap);
    return checkPowerAndPerformance(usPeriod);
}

// TODO: try to exclude printing from ECO
static inline
void printLine() {
    std::cout << "-----------------------------------------------"
              << "---------------------------------------------\n"; 
}

static inline
void printHeader() {
    printLine();
    std::cout << "Cap\tE\tP\tfilterP\tips\tmin(E)\tmin(Et)\tEDS\n";
    printLine();
}

static inline
void logCurrentRangeGSS(int a, int leftCandidate, int rightCandidate, int b) {
    std::cout << "--------------------------------\n"
              << "Current GSS range: |"
              << a/1000000 << " "
              << leftCandidate/1000000 << " "
              << rightCandidate/1000000 << " "
              << b/1000000 << "|\n"
              << "--------------------------------\n";
}

void Eco::reportResult(double waitTime, double testTime) {
    auto&& totalE = devStateGlobal_.getEnergySinceReset(Domain::PKG);
    auto&& totalTime = devStateGlobal_.getTimeSinceReset<std::chrono::seconds>();
    std::cout << "Total E: " << totalE;
    // ----------------------------------------------------------------------------------------
    // Below code is temporary disabled as it uses method specific to some
    // class of systems with two Intel CPUs treated as a single device.
    // Ultimately the Device shall represent single CPU and the energy readings
    // shall be combined in DeviceStateAccumulator. If a system consist of
    // multiple CPUs used by the same application it shall be reflected as multiple
    // Device object instances within DeviceStateAccumulator.
    //
    //  || || || || || || || || || || || || || || || || ||
    //  \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/ \/
    //
    // auto&& energyVec = devStateGlobal_.getTotalEnergyVec(Domain::PKG);
    // int pkgID = 0;
    // for (auto&& pkgE : energyVec) {
    //     pkgID++;
    //     std::cout << "\nPKG" << pkgID << ": " << pkgE << ", (" << (pkgE/totalE)*100 << "%)";
    // }
    // ----------------------------------------------------------------------------------------
    std::cout << "\nTotal P: " << totalE / totalTime <<
                 "\nTotal t: " << totalTime << "s\n";
    if (waitTime != 0.0 || testTime != 0.0) {
        std::cout << "Wait time: " << waitTime << "s, (" << (waitTime/totalTime)*100 << "%)\n"
                  << "Test time: " << testTime << "s, (" << (testTime/totalTime)*100 << "%)\n";
    }
}

void Eco::waitPhase(int& status, int childPID) {
    waitpid(childPID, &status, WNOHANG);
    optimizationTrigger_ = false; // reset trigger before workload starts
    while (!optimizationTrigger_ && status) {
        // wait until application runs homogenous workload (skip computation start up)
        auto papResult = checkPowerAndPerformance(cfg_.usTestPhasePeriod_);
        std::cout << FLUSH_AND_RETURN
                  << logCurrentResultLine(papResult, papResult, cfg_.k_, true /* no new line */);
    }
    std::cout << "\n";
    printLine();
}

int Eco::testPhase(
    int& highLimit_uW,
    int& lowLimit_uW,
    TargetMetric metric,
    SearchType searchType,
    PowAndPerfResult& refResult)
{
    highLimit_uW = adjustHighPowLimit(refResult, highLimit_uW);
    std::cout << logCurrentResultLine(refResult, refResult, cfg_.k_);
    printLine();

    switch (searchType) {
        case SearchType::LINEAR_SEARCH :
            return linearSearchForBestPowerCap(refResult,
                                               highLimit_uW,
                                               lowLimit_uW,
                                               metric);
        case SearchType::GOLDEN_SECTION_SEARCH :
            return goldenSectionSearchForBestPowerCap(refResult,
                                                      highLimit_uW,
                                                      lowLimit_uW,
                                                      metric);
        default:
            return highLimit_uW;
    }
}

void Eco::execPhase(
    int powerCap_uW,
    int& status,
    int childPID,
    PowAndPerfResult& refResult)
{
    device_->setPowerLimitInMicroWatts(powerCap_uW);
    printLine();
    while (status) {
        auto papResult = checkPowerAndPerformance(cfg_.usTestPhasePeriod_);

        std::cout << FLUSH_AND_RETURN;
        std::cout << logCurrentResultLine(papResult, refResult, cfg_.k_, true /* no new line */);
        waitpid(childPID, &status, WNOHANG);
    }
    std::cout << "\n";
    printLine();
}

int& Eco::adjustHighPowLimit(PowAndPerfResult firstResult, int& currHighLimit_uW)
{
    // check if default power cap is higher than max power cap (TDP)
    // if (currHighLimit_uW < device_->getDefaultCaps().defaultConstrPKG_->longPower) {
    //     currHighLimit_uW = device_->getDefaultCaps().defaultConstrPKG_->longPower;
    if (currHighLimit_uW < defaultPowerLimitInWatts_) {
        currHighLimit_uW = defaultPowerLimitInWatts_;
    }
    // reduce starting high power limit according to first result's average power
    if ((firstResult.averageCorePowerInWatts_ * 1000000) < (currHighLimit_uW/2)) {
        currHighLimit_uW = (firstResult.averageCorePowerInWatts_ * 1000000) * 2.0;
    }
    return currHighLimit_uW;
}

void Eco::mainAppProcess(char* const* argv, int& stdoutFileDescriptor)
{
    if (dup2(stdoutFileDescriptor, 1) < 0) {
        perror("dup2");
        abort();
    }
    close(stdoutFileDescriptor);

    int execStatus = execvp(argv[1], argv+1);
    validateExecStatus(execStatus);
}

int Eco::linearSearchForBestPowerCap(
    PowAndPerfResult& firstResult,
    int& highLimit_uW,
    int& lowLimit_uW,
    TargetMetric metric)
{
    int bestCap = highLimit_uW;
    auto currBestResult = firstResult;
    int step = ((highLimit_uW - lowLimit_uW)/ 100) * cfg_.percentStep_;

    for (int limit_uW = lowLimit_uW ; limit_uW < highLimit_uW; limit_uW += step) {
        auto papResult = setCapAndMeasure(limit_uW, cfg_.usTestPhasePeriod_);
        std::cout << logCurrentResultLine(papResult, firstResult, cfg_.k_);
        if (currBestResult.isRightBetter(papResult, metric)) {
            currBestResult = std::move(papResult);
            bestCap = limit_uW;
        }
    }
    return bestCap;
}

int Eco::goldenSectionSearchForBestPowerCap(
    PowAndPerfResult& firstResult,
    int& highLimit_uW,
    int& lowLimit_uW,
    TargetMetric metric)
{
    int EPSILON = (highLimit_uW - lowLimit_uW)/ 100;
    std::cout << "EPSILON: " << EPSILON/1000000 << "\n";
    int a = lowLimit_uW;
    int b = highLimit_uW;
    float phi = (sqrt(5) - 1) / 2; // this is equal 0.618 and it is reverse of 1.618
    int leftCandidate = b - int(phi * (b - a));
    int rightCandidate = a + int(phi * (b - a));
    logCurrentRangeGSS(a, leftCandidate, rightCandidate, b);
    bool measureL = true;
    bool measureR = true;
    PowAndPerfResult tmp;
    while ((b - a) > EPSILON) {
        auto fL = measureL ? setCapAndMeasure(leftCandidate, cfg_.usTestPhasePeriod_) : tmp;
        std::cout << logCurrentResultLine(fL, firstResult, cfg_.k_);
        auto fR = measureR ? setCapAndMeasure(rightCandidate, cfg_.usTestPhasePeriod_) : tmp;
        std::cout << logCurrentResultLine(fR, firstResult, cfg_.k_);

        if (!fL.isRightBetter(fR, metric)) {
            // choose subrange [a, rightCandidate]
            b = rightCandidate;
            rightCandidate = leftCandidate;
            tmp = fL;
            measureR = false;
            measureL = true;
            leftCandidate = b - int(phi * (b - a));
        } else {
            // choose subrange [leftCandidate, b]
            a = leftCandidate;
            leftCandidate = rightCandidate;
            tmp = fR;
            measureR = true;
            measureL = false;
            rightCandidate = a + int(phi * (b - a));
        }
        logCurrentRangeGSS(a, leftCandidate, rightCandidate, b);
    }
    // when stop condition was met return the middle of the subrange found
    return (a + b) / 2;
}

FinalPowerAndPerfResult Eco::runAppWithSearch(
    char* const* argv,
    TargetMetric targerMetric,
    SearchType searchType,
    int argc)
{
    // this is redirecting the original output of the tuned application to txt file
    int fd = open("redirected.txt", O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }
    // ----------------------------------------------------------------------------

    devStateGlobal_.resetState();

    double waitTime = 0.0, testTime = 0.0;
    pid_t childProcId = fork();
    if (childProcId >= 0) //fork successful
    {
        if (childProcId == 0) // child process
        {
            mainAppProcess(argv, fd);
        }
        else  // parent process
        {
            int lowPowLimit_uW = (int)(idleAvPow_[PowerCapDomain::PKG] * 1000000 + 0.5); // add 0.5 to round-up double while casting to int
            int highPowLimit_uW = device_->getMinMaxLimitInWatts().second * 1000000;//(int)(devStateGlobal_.getPkgMaxPower() * 1000000 + 0.5); // same
            int status = 1;
            printHeader();
            waitTime = measureDuration([&, this] {
                waitPhase(status, childProcId);
            });
            //----------------------------------------------------------------------------
            auto firstResult = checkPowerAndPerformance(FIRST_RUN_MULTIPLIER * cfg_.usTestPhasePeriod_);
            int bestCap;
            testTime = measureDuration([&, this] {
                bestCap = testPhase(highPowLimit_uW,
                                    lowPowLimit_uW,
                                    targerMetric,
                                    searchType,
                                    firstResult);
            });
            execPhase(bestCap, status, childProcId, firstResult);
            // device_->restoreDefaults();
            device_->setPowerLimitInMicroWatts(10e6 * defaultPowerLimitInWatts_);
        }
    }
    else
    {
        std::cerr << "fork failed" << std::endl;
        // TODO: handle errors
        // return 1;
    }
    reportResult(waitTime, testTime);
    double totalTimeInSeconds = devStateGlobal_.getTimeSinceReset<std::chrono::seconds>();

    return FinalPowerAndPerfResult(device_->getMinMaxLimitInWatts().second,
                                devStateGlobal_.getEnergySinceReset(Domain::PKG),
                                devStateGlobal_.getEnergySinceReset(Domain::PKG) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::PP0) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::PP1) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::DRAM) / totalTimeInSeconds,
                                TimeResult(totalTimeInSeconds, waitTime, testTime),
                                0.0,
                                0.0);
}


FinalPowerAndPerfResult Eco::runAppWithSampling(char* const* argv, int argc) {
    singleAppRunAndPowerSample(argv);
    reportResult();
    double totalTimeInSeconds = devStateGlobal_.getTimeSinceReset<std::chrono::seconds>();

    return FinalPowerAndPerfResult(device_->getMinMaxLimitInWatts().second,
                                devStateGlobal_.getEnergySinceReset(Domain::PKG),
                                devStateGlobal_.getEnergySinceReset(Domain::PKG) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::PP0) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::PP1) / totalTimeInSeconds,
                                devStateGlobal_.getEnergySinceReset(Domain::DRAM) / totalTimeInSeconds,
                                totalTimeInSeconds,
                                devStateGlobal_.getPerfCounterSinceReset(),
                                0.0 // num of cycles is not needed, so might be removed in the future
                                );
}

FinalPowerAndPerfResult Eco::multipleAppRunAndPowerSample(char* const* argv, int numIterations) {
    FinalPowerAndPerfResult sum;
    for(auto i = 0; i < cfg_.numIterations_; i++) {
        sum += runAppWithSampling(argv);
        std::cout << "*" << std::flush;
    }
    std::cout << FLUSH_AND_RETURN;
    sum /= cfg_.numIterations_;
    return sum;
}

void Eco::storeReferenceRun(FinalPowerAndPerfResult& ref) {
    if (fullAppRunResultsContainer_.size() == 0) {
        fullAppRunResultsContainer_.emplace_back(device_->getMinMaxLimitInWatts().second,
                                        ref.energy,
                                        ref.pkgPower,
                                        ref.pp0power,
                                        ref.pp1power,
                                        ref.dramPower,
                                        ref.time_,
                                        ref.inst,
                                        ref.cycl);
        std::cout << "Reference run saved.\n";
    } else {
        std::cout << "Reference run already exists!.\n";
    }
}

void Eco::referenceRunWithoutCaps(char* const* argv) {
    auto avResult = multipleAppRunAndPowerSample(argv, cfg_.numIterations_);
    fullAppRunResultsContainer_.emplace_back(defaultPowerLimitInWatts_, //devStateGlobal_.getPkgMaxPower() + 1, //this is temporary value for "default" powercap
                                    avResult.energy,
                                    avResult.pkgPower,
                                    avResult.pp0power,
                                    avResult.pp1power,
                                    avResult.dramPower,
                                    avResult.time_,
                                    avResult.inst,
                                    avResult.cycl);
    std::cout << "Reference run saved.\n";
}

void Eco::runAppForEachPowercap(char* const* argv, BothStream& stream, Domain dom) {
    // This is legacy method which requires prior referenceRunWithoutCaps() method call.
    // This method was used when there was a need for static
    // evaluation of different power caps for a given power domain.
    // In recent Intel based CPUs there is no PP0 and PP1 domains so that
    // it is really only a matter if one want to limit PKG domain or DRAM.
    // The method will be probably removed soon.
    // For now the tool supports only PKG domain and static energy profiling is covered
    // end-to-end by staticEnergyProfiler() method of ECO class.
    // if (device_->isDomainAvailable(dom))
    // {
    //     stream << "#[INFO] Running tests for domain " << dom << ".\n";
    //     const auto ref = fullAppRunResultsContainer_.front(); //TODO: remove the dependency on reference run
    //     stream << std::fixed << std::setprecision(3) << ref << "\n";
    //     auto powerLimitsVec = generateVecOfPowerCaps(dom);
    //     auto loopsToDo = powerLimitsVec.size();
    //     for (auto& currentLimit : powerLimitsVec) {
    //         std::cout << loopsToDo-- << " ";
    //         device_->setPowerLimitInMicroWatts(currentLimit, dom);
    //         auto avResult = multipleAppRunAndPowerSample(argv, cfg_.numIterations_);
    //         auto k = getK();
    //         auto mPlus = EnergyTimeResult(avResult.energy,
    //                                       avResult.time_.totalTime_,
    //                                       avResult.pkgPower).checkPlusMetric(ref.getEnergyAndTime(), k);
    //         auto mPlusDynamic = (1.0/k) * (ref.getInstrPerSec() / avResult.getInstrPerSec()) *
    //                             ((k-1.0) * (avResult.getEnergyPerInstr() / ref.getEnergyPerInstr()) + 1.0);
    //         auto&& timeDelta = avResult.time_.totalTime_ - ref.time_.totalTime_;
    //         fullAppRunResultsContainer_.emplace_back((double)currentLimit / 1000000,
    //                                         avResult.energy,
    //                                         avResult.pkgPower,
    //                                         avResult.pp0power,
    //                                         avResult.pp1power,
    //                                         avResult.dramPower,
    //                                         avResult.time_.totalTime_,
    //                                         avResult.inst,
    //                                         avResult.cycl,
    //                                         avResult.energy - ref.energy,
    //                                         timeDelta,
    //                                         100 * (avResult.energy - ref.energy) / ref.energy,
    //                                         100 * (timeDelta) / ref.time_.totalTime_,
    //                                         mPlus);
    //         stream << fullAppRunResultsContainer_.back() << "\t" << mPlusDynamic << "\n";
    //         if (fullAppRunResultsContainer_.back().relativeDeltaT > (double)cfg_.perfDropStopCondition_) {
    //             break;
    //         }
    //     }
    //     device_->restoreDefaults();
    //     stream << "# PowerCap for: min(E): "
    //            << std::min_element(fullAppRunResultsContainer_.begin(),
    //                                fullAppRunResultsContainer_.end(),
    //                                CompareFinalResultsForMinE())->powercap
    //            << " W, "
    //            << "min(Et): "
    //            << std::min_element(fullAppRunResultsContainer_.begin(),
    //                                fullAppRunResultsContainer_.end(),
    //                                CompareFinalResultsForMinEt())->powercap
    //            << " W, "
    //            << "min(M+): "
    //            << std::min_element(fullAppRunResultsContainer_.begin(),
    //                                fullAppRunResultsContainer_.end(),
    //                                CompareFinalResultsForMplus())->powercap
    //            << " W.\n";
    // } else {
    //     stream << "[INFO] Domain " << dom << " is not supproted.\n";
    // }
}

WatchdogStatus Eco::readWatchdog() {
    if (readLimitFromFile("/proc/sys/kernel/nmi_watchdog") > 0) {
        return WatchdogStatus::ENABLED;
    } else {
        return WatchdogStatus::DISABLED;
    }
}

void Eco::modifyWatchdog(WatchdogStatus status) {
    std::ofstream watchdog ("/proc/sys/kernel/nmi_watchdog", std::ios::out | std::ios::trunc);
    watchdog << (status == WatchdogStatus::ENABLED) ? "1" : "0";
    watchdog.close();
}

void Eco::plotPowerLog() {
    std::string imgFileName = outPowerFileName_;
    PlotBuilder p(imgFileName.replace(imgFileName.end() - 3,
                                      imgFileName.end(),
                                      "png"));
    p.setPlotTitle("Power Log - " + getDeviceName());
    Series powerCap (outPowerFileName_, 1, 2, "P cap [W]");
    Series currPKGpower (outPowerFileName_, 1, 3, "current PKG P[W]");
    Series currSMA50power (outPowerFileName_, 1, 4, "SMA50 PKG P[W]");
    Series currSMA100power (outPowerFileName_, 1, 5, "filtered PKG P[W]");
    Series currSMA200power (outPowerFileName_, 1, 6, "SMA200 PKG P[W]");
    Series currDRAMpower (outPowerFileName_, 1, 7, "current DRAM P[W]");
    p.plotPowerLog({powerCap,
                    currPKGpower,
                    currDRAMpower,
                    currSMA100power});
}

void Eco::staticEnergyProfiler(char* const* argv, BothStream& stream) {
    PowerCapDomain dom = PowerCapDomain::PKG;
    std::vector<FinalPowerAndPerfResult> resultsVec;
    const auto&& warmup = runAppWithSampling(argv);
    stream << "# " << std::fixed << std::setprecision(3) << warmup << "\n";
    stream << "# warmup done #\n";
    //
    FinalPowerAndPerfResult reference;
    for(auto i = 0; i < cfg_.numIterations_; i++)
    {
        auto tmp = runAppWithSampling(argv);
        reference += tmp;
        stream << "# " << std::fixed << std::setprecision(3) << tmp << "\n";
    }
    reference /= cfg_.numIterations_;
    //
    resultsVec.push_back(reference);
    stream << reference << "\n";
    auto powerLimitsVec = generateVecOfPowerCaps(dom);
    auto loopsToDo = powerLimitsVec.size();
    for (auto& currentLimit : powerLimitsVec) {
        std::cout << loopsToDo-- << " ";
        device_->setPowerLimitInMicroWatts(currentLimit);
        auto avResult = multipleAppRunAndPowerSample(argv, cfg_.numIterations_);
        auto k = getK();
        auto mPlus = EnergyTimeResult(avResult.energy,
                                        avResult.time_.totalTime_,
                                        avResult.pkgPower).checkPlusMetric(reference.getEnergyAndTime(), k);
        auto mPlusDynamic = (1.0/k) * (reference.getInstrPerSec() / avResult.getInstrPerSec()) *
                            ((k-1.0) * (avResult.getEnergyPerInstr() / reference.getEnergyPerInstr()) + 1.0);
        auto&& timeDelta = avResult.time_.totalTime_ - reference.time_.totalTime_;
        resultsVec.emplace_back((double)currentLimit / 1000000,
                                 avResult.energy,
                                 avResult.pkgPower,
                                 avResult.pp0power,
                                 avResult.pp1power,
                                 avResult.dramPower,
                                 avResult.time_.totalTime_,
                                 avResult.inst,
                                 avResult.cycl,
                                 avResult.energy - reference.energy,
                                 timeDelta,
                                 100 * (avResult.energy - reference.energy) / reference.energy,
                                 100 * (timeDelta) / reference.time_.totalTime_,
                                 mPlus);
        stream << resultsVec.back() << "\t" << mPlusDynamic << "\n";
        if (resultsVec.back().relativeDeltaT > (double)cfg_.perfDropStopCondition_) {
            break;
        }
    }
    // device_->restoreDefaults();
    device_->setPowerLimitInMicroWatts(10e6 * defaultPowerLimitInWatts_);
    stream << "# PowerCap for: min(E): "
            << std::min_element(resultsVec.begin(),
                                resultsVec.end(),
                                CompareFinalResultsForMinE())->powercap
            << " W, "
            << "min(Et): "
            << std::min_element(resultsVec.begin(),
                                resultsVec.end(),
                                CompareFinalResultsForMinEt())->powercap
            << " W, "
            << "min(M+): "
            << std::min_element(resultsVec.begin(),
                                resultsVec.end(),
                                CompareFinalResultsForMplus())->powercap
            << " W.\n";
}