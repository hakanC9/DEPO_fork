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

#include "gpu_eco.hpp"

static inline
void logCurrentRangeGSS(int a, int leftCandidateInMilliWatts, int rightCandidateInMilliWatts, int b)
{
    std::cout << "#--------------------------------\n"
              << "# Current GSS range: |"
              << a << " "
              << leftCandidateInMilliWatts << " "
              << rightCandidateInMilliWatts << " "
              << b << "|\n"
              << "#--------------------------------\n";
}

static inline
std::string parseArgvCommandToString(int argc, char** argv)
{
    std::stringstream str;
    str << "# ";
    for (int i=1; i < argc; i++) {
        str << argv[i] << " ";
    }
    str << "\n";
    return str.str();
}

static inline
std::string generateUniqueDir()
{
    std::string dir = "gpu_experiment_" +
        std::to_string(
            std::chrono::system_clock::to_time_t(
                  std::chrono::high_resolution_clock::now()));
    dir += "/";
    const int dir_err = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == dir_err) {
        printf("Error creating experiment result directory!\n");
        exit(1);
    }
    return dir;
}

static inline
int long long readValueFromFile (std::string fileName)
{
    std::ifstream file (fileName.c_str());
    std::string line;
    long long int value = -1;
    if (file.is_open())
    {
        while ( getline (file, line) )
        {
            value = atoi(line.c_str());
        }
        file.close();
    }
    else
    {
        std::cerr << "cannot read the value from file: " << fileName << "\n"
                  << "file not open\n";
    }
    return value;
}

CudaDevice::CudaDevice(int devID) :
    deviceID_(devID) // and then this field shall not be a member of this class as the API allows for access to any device
{
    std::cout << "[DEBUG]: CudaDevice constructor called!\n";
    int major;
    CUresult result;
    CUdevice device {deviceID_};

    result = cuInit(0);
    if (result != CUDA_SUCCESS)
    {
        printf("Error code %d on cuInit\n", result);
        exit(-1);
    }
    result = cuDeviceGet(&device,0);
    if (result != CUDA_SUCCESS)
    {
        printf("Error code %d on cuDeviceGet\n", result);
        exit(-1);
    }

    result = cuDeviceGetAttribute (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
    if (result != CUDA_SUCCESS)
    {
        printf("Error code %d on cuDeviceGetAttribute\n", result);
        exit(-1);
    }
    // NVML INITIALIZATIONS
    nvResult_ = nvmlInit();
    if (NVML_SUCCESS != nvResult_)
    {
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(nvResult_));
        return;
    }
    nvResult_ = nvmlDeviceGetCount(&deviceCount_);
    if (NVML_SUCCESS != nvResult_)
    {
        printf("Failed to query device count: %s\n", nvmlErrorString(nvResult_));
        return;
    }
    printf("Found %d device%s\n\n", deviceCount_, deviceCount_ != 1 ? "s" : "");
    initDeviceHandles();
    std::cout << "DEBUG device handles initialized succesfully" << std::endl;
    defaultPowerLimitInWatts_ = this->getPowerLimitInWatts();
}

double CudaDevice::getPowerLimitInWatts() const
{
    unsigned currPowerLimitInMilliWatts = 0;
    nvmlReturn_t nvResult = nvmlDeviceGetEnforcedPowerLimit (deviceHandles_[deviceID_], &currPowerLimitInMilliWatts);
    if (NVML_SUCCESS != nvResult)
    {
        printf("Failed to GET current power limit: %s\n", nvmlErrorString(nvResult));
        return -1;
    }
    return (double)currPowerLimitInMilliWatts / 1000;
}

std::string CudaDevice::getName() const
{
    constexpr unsigned maxLength = 96;
    char name[maxLength];
    nvmlReturn_t nvResult = nvmlDeviceGetName (deviceHandles_[deviceID_], name, maxLength);
    if (NVML_SUCCESS != nvResult)
    {
        printf("Failed to GET device name: %s\n", nvmlErrorString(nvResult));
        return std::string("Unknown GPU");
    }
    return std::string(name);
}

std::pair<unsigned, unsigned> CudaDevice::getMinMaxLimitInWatts() const
{
    unsigned min = 0, max = 0;
    nvmlReturn_t nvResult = nvmlDeviceGetPowerManagementLimitConstraints (deviceHandles_[deviceID_], &min, &max);
    if (NVML_SUCCESS != nvResult)
    {
        printf("Failed to GET min/max power limit: %s\n", nvmlErrorString(nvResult));
    }
    return std::make_pair(min/1000, max/1000);
}

void CudaDevice::setPowerLimitInMicroWatts(unsigned long limitInMicroW)
{
    unsigned long limitInMilliWatts = limitInMicroW / 1e3;
    nvmlReturn_t nvResult = nvmlDeviceSetPowerManagementLimit (deviceHandles_[deviceID_], limitInMilliWatts);
    if (NVML_SUCCESS != nvResult)
    {
        printf("Failed to SET current power limit %ld [mW]: %s\n", limitInMilliWatts, nvmlErrorString(nvResult));
        return;
    }
}

void CudaDevice::reset()
{
    std::ofstream kernelCounterFile;
    kernelCounterFile.open("kernels_count", std::ios::out | std::ios::trunc);
    kernelCounterFile << "0";
    kernelCounterFile.close();
}

double CudaDevice::getCurrentPowerInWatts(std::optional<Domain>) const
{
    unsigned power;
    nvmlReturn_t nvResult = nvmlDeviceGetPowerUsage(deviceHandles_[deviceID_], &power);
    if (NVML_SUCCESS != nvResult)
    {
        printf("Failed to get power usage: %s\n", nvmlErrorString(nvResult));
        return -1.0;
    }
    return (double)power/1000.0;
}

void CudaDevice::initDeviceHandles()
{
    nvmlDevice_t nvDevice;
    nvmlReturn_t nvResult;
    deviceHandles_.resize(deviceCount_);
    for (unsigned i = 0; i < deviceCount_; i++)
    {
        nvResult = nvmlDeviceGetHandleByIndex(i, &nvDevice);
        if (NVML_SUCCESS != nvResult)
        {
            printf("Failed to get handle for device 1: %s\n", nvmlErrorString(nvResult));
            return;
        }
        deviceHandles_[i] = nvDevice;
    }
}

unsigned long long int CudaDevice::getPerfCounter() const
{
    long long int kernelsCountTmp {-1};
    do
    {
        kernelsCountTmp = readValueFromFile("./kernels_count");
    }
    while (kernelsCountTmp == -1);

    return (unsigned long long)kernelsCountTmp;
}

void CudaDevice::restoreDefaultLimits()
{
    setPowerLimitInMicroWatts(1e6 * defaultPowerLimitInWatts_);

}
// GpuDeviceState::GpuDeviceState(std::shared_ptr<CudaDevice>& device) :
//     absoluteStartTime_(std::chrono::high_resolution_clock::now()),
//     timeOfLastReset_(std::chrono::high_resolution_clock::now()),
//     gpu_(device),
//     prev_(0.0, 0, timeOfLastReset_),
//     curr_(prev_),
//     next_(prev_)
// {
//     // prev_ = curr_ = next_ = PowerAndPerfState(0.0, 0,);
//     sample();
//     sample();
//     std::cout << "DEBUG device state initialized succesfully" << std::endl;
// }


// GpuDeviceState& GpuDeviceState::sample()
// {
//     prev_ = curr_;
//     curr_ = next_;

//     const auto kernelsCounter = gpu_->getPerfCounter();

//     next_ = PowerAndPerfState(
//         gpu_->getCurrentPowerInWatts(),
//         kernelsCounter,
//         std::chrono::high_resolution_clock::now());

//     auto timeDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(next_.time_ - curr_.time_).count();
//     totalEnergySinceReset_ += next_.power_ * timeDeltaMs / 1000;
//     return *this;
// }

// PowAndPerfResult GpuDeviceState::getCurrentPowerAndPerf(int deviceID) const
// {
//     double timeDeltaMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(next_.time_ - curr_.time_).count();
//     return PowAndPerfResult(
//         (double)(next_.kernelsCount_ - curr_.kernelsCount_),
//         timeDeltaMilliSeconds / 1000,
//         gpu_->getPowerLimitInWatts(),
//         next_.power_ * timeDeltaMilliSeconds / 1000, // Watts x seconds
//         next_.power_,
//         0.0, // memory power - not available for GPU
//         next_.power_ // TODO: this should be filtered power
//         );
// }

// double GpuDeviceState::getEnergySinceReset() const
// {
//     return totalEnergySinceReset_;
// }

// void GpuDeviceState::resetState()
// {
//     timeOfLastReset_ = std::chrono::high_resolution_clock::now();
//     totalEnergySinceReset_ = 0.0;
//     gpu_->reset();
//     sample();
//     sample();
// }


GpuEco::GpuEco(int deviceID) : logger_("gpu_"), trigger_(TriggerType::NO_TUNING)
{
    gpu_ = std::make_shared<CudaDevice>(deviceID);
    deviceState_ = std::make_unique<DeviceStateAccumulator>(gpu_);
    std::tie(minPowerLimit_, maxPowerLimit_) = gpu_->getMinMaxLimitInWatts();
    gpu_->reset();
    // const auto dir = generateUniqueDir();
    // outPowerFileName_ = dir + "power_log.csv";
    // outResultFileName_ = dir + "result.csv";
    // outPowerFile_.open(outPowerFileName_, std::ios::out | std::ios::trunc);
    // bout_ = std::make_unique<BothStream>(outPowerFile_);
}

GpuEco::~GpuEco()
{
    gpu_->restoreDefaultLimits();
    // outPowerFile_.close();
}

void GpuEco::idleSample(int sleepPeriodInMs)
{
    usleep(sleepPeriodInMs * 1000);
    deviceState_->sample();
    auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(trigger_);
    // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), tmpResult, tmpResult);
    logger_.logPowerLogLine(*deviceState_, tmpResult);
    }

FinalPowerAndPerfResult GpuEco::runAppWithSampling(char* const* argv, int argc)
{
    std::string command;
    for (int i=1; i < argc; i++)
    {
        command += argv[i];
        command += " ";
    }
    deviceState_->resetState();
    int status = 1;
    pid_t childProcId = fork();
    if (childProcId >= 0)
    {
        if (childProcId == 0)
        {
            std::cout << "||||| my pid " << getpid() << " parent " << getppid() << std::endl;
            int exec_status = system(command.c_str());

            validateExecStatus(exec_status);
            exit(0);
        }
        else
        {
            while(status)
            {
                idleSample(cfg_.msPause_);
                waitpid(childProcId, &status, WNOHANG);
            }
            wait(&status);
        }
    }
    else
    {
        std::cerr << "fork failed" << std::endl;
        //return 1;
    }
    const auto&& execTime = deviceState_->getTimeSinceReset<std::chrono::milliseconds>() / 1000;
    return FinalPowerAndPerfResult(
        gpu_->getPowerLimitInWatts(),
        deviceState_->getEnergySinceReset(),
        deviceState_->getEnergySinceReset() / execTime,
        0.0, //PP0
        0.0, //PP1
        0.0, //DRAM
        TimeResult(execTime, 0.0, 0.0),
        0.0, //instr
        0.0  //cycl
        );
}

void GpuEco::staticEnergyProfiler(char* const* argv, int argc, BothStream& stream)
{
    std::vector<FinalPowerAndPerfResult> oneSeriesResultVec;
    const auto&& warmup = runAppWithSampling(argv, argc);
    stream << "# " << std::fixed << std::setprecision(3) << warmup << "\n";
    stream << "# warmup done #\n";
    //
    FinalPowerAndPerfResult reference;
    for(auto i = 0; i < cfg_.numIterations_; i++)
    {
        auto tmp = runAppWithSampling(argv, argc);
        reference += tmp;
        stream << "# " << std::fixed << std::setprecision(3) << tmp << "\n";
    }
    reference /= cfg_.numIterations_;
    //
    oneSeriesResultVec.push_back(reference);
    stream << reference << "\n";
    for (unsigned limitInWatts = maxPowerLimit_; limitInWatts >= minPowerLimit_; limitInWatts -= 5)
    {
        gpu_->setPowerLimitInMicroWatts(limitInWatts * 10e6);
        // average for numIterations_ number of runs
        FinalPowerAndPerfResult current;
        for(auto i = 0; i < cfg_.numIterations_; i++) {
            auto tmp = runAppWithSampling(argv, argc);
            current += tmp;
            stream << "# " << std::fixed << std::setprecision(3) << tmp << "\n";
        }
        current /= cfg_.numIterations_;
        //
        // auto&& current = runAppWithSampling(argv, argc);
        auto k = getK();
        auto mPlus = EnergyTimeResult(current.energy,
                                        current.time_.totalTime_,
                                        current.pkgPower).checkPlusMetric(reference.getEnergyAndTime(), k);
        auto mPlusDynamic = (1.0/k) * (reference.getInstrPerSec() / current.getInstrPerSec()) *
                            ((k-1.0) * (current.getEnergyPerInstr() / reference.getEnergyPerInstr()) + 1.0);
        auto&& timeDelta = current.time_.totalTime_ - reference.time_.totalTime_;
        oneSeriesResultVec.emplace_back(gpu_->getPowerLimitInWatts(),
                                        current.energy,
                                        current.pkgPower,
                                        current.pp0power,
                                        current.pp1power,
                                        current.dramPower,
                                        current.time_.totalTime_,
                                        current.inst,
                                        current.cycl,
                                        current.energy - reference.energy,
                                        timeDelta,
                                        100 * (current.energy - reference.energy) / reference.energy,
                                        100 * (timeDelta) / reference.time_.totalTime_,
                                        mPlus);
        stream << oneSeriesResultVec.back() << "\t" << mPlusDynamic << "\n";
    }
    stream << "# PowerCap for: min(E): "
    << std::min_element(oneSeriesResultVec.begin(),
                        oneSeriesResultVec.end(),
                        CompareFinalResultsForMinE())->powercap
    << " W, "
    << "min(Et): "
    << std::min_element(oneSeriesResultVec.begin(),
                        oneSeriesResultVec.end(),
                        CompareFinalResultsForMinEt())->powercap
    << " W, "
    << "min(M+): "
    << std::min_element(oneSeriesResultVec.begin(),
                        oneSeriesResultVec.end(),
                        CompareFinalResultsForMplus())->powercap
    << " W.\n";
}

FinalPowerAndPerfResult GpuEco::runAppWithSearch(
    char* const* argv,
    TargetMetric metric,
    SearchType searchType,
    int argc)
{
    // std::string command;
    // for (int i=1; i < argc; i++)
    // {
    //     command += argv[i];
    //     command += " ";
    // }
    // this is redirecting the original output of the tuned application to txt file
    int fd = open("redirected.txt", O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); abort(); }
    // ----------------------------------------------------------------------------
    deviceState_->resetState();

    double waitTime = 0.0, testTime = 0.0;
    int bestResultCapInMicroWatts = 300;
    pid_t childProcId = fork();
    if (childProcId >= 0)
    {
        if (childProcId == 0)
        {
            // std::cout << "ENV1 value: " << getenv("INJECTION_KERNEL_COUNT")
            //       << "\nENV2 value: " << getenv("CUDA_INJECTION64_PATH") << "\n";
            // std::cout << "||||| my pid " << getpid() << " parent " << getppid() << std::endl;
            // int execStatus = system(command.c_str());
            // int execStatus = execvp(argv[1], argv+1);

            // validateExecStatus(execStatus);
            if (dup2(fd, 1) < 0) {
                perror("dup2");
                abort();
            }
            close(fd);

            int execStatus = execvp(argv[1], argv+1);
            validateExecStatus(execStatus);
            // exit(0);
        }
        else
        {
            int status = 1;
            waitpid(childProcId, &status, WNOHANG);
            waitTime = measureDuration([&, this] {
                waitForGpuComputeActivity(status, cfg_.msPause_);
            });
            PowAndPerfResult referenceRun;
            testTime = measureDuration([&, this] {

                referenceRun = getReferenceResult(cfg_.msTestPhasePeriod_);

                Algorithm algorithm;
                if (searchType == SearchType::LINEAR_SEARCH)
                {
                    algorithm = LinearSearchAlgorithm();
                }
                else
                {
                    algorithm = GoldenSectionSearchAlgorithm();
                }
                bestResultCapInMicroWatts = algorithm(gpu_, *deviceState_, trigger_, metric, referenceRun, status, childProcId, cfg_.msPause_, cfg_.msTestPhasePeriod_, logger_);
            });
            executeWithPowercap(status, bestResultCapInMicroWatts, cfg_.msPause_, childProcId, referenceRun);
            gpu_->restoreDefaultLimits();
            wait(&status);
        }
    }
    else
    {
        std::cerr << "fork failed" << std::endl;
        //return 1;
    }
    const auto&& execTime = deviceState_->getTimeSinceReset<std::chrono::milliseconds>() / 1000;
    return FinalPowerAndPerfResult(
        bestResultCapInMicroWatts / 1.0e6,
        deviceState_->getEnergySinceReset(),
        deviceState_->getEnergySinceReset() / execTime,
        0.0,
        0.0,
        0.0,
        TimeResult(execTime, waitTime, testTime),
        0.0,
        0.0);
}

void GpuEco::plotPowerLog(std::optional<FinalPowerAndPerfResult> results)
{
    // bout_->flush();
    // outPowerFile_.close();
    logger_.flushAndClose();
    const auto f = logger_.getPowerFileName();
    // std::cout << "Processing " << outPowerFileName_ << " file...\n";
    std::cout << "Processing " << f << " file...\n";
    // std::string imgFileName = outPowerFileName_;
    std::string imgFileName = f;
    PlotBuilder p(imgFileName.replace(imgFileName.end() - 4,
                                    imgFileName.end(),
                                    ".png"));
    p.setPlotTitle("Power log - " + getDeviceName(), 16);
    if(results)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3)
        << "Total E: " << results->energy << " J"
        << "    Total time: " << results->time_.totalTime_ << " s"
        << "    av. Power: " << results->pkgPower << " W";
        // << "    last Powercap: " << results->powercap << " W";
        p.setSimpleSubtitle(ss.str(), 12);
    }
    // Series powerCap (outPowerFileName_, 1, 2, "P cap [W]");
    // Series currPower (outPowerFileName_, 1, 3, "P[W]");
    // Series currENG (outPowerFileName_, 1, 7, "ENG");
    // Series currEDP (outPowerFileName_, 1, 8, "EDP");
    // Series currPERF (outPowerFileName_, 1, 9, "PERF");
    Series powerCap (f, 1, 2, "P cap [W]");
    Series currPower (f, 1, 3, "P[W]");
    Series smaPower (f, 1, 4, "P[W]");
    Series currENG (f, 1, 7, "ENG");
    Series currEDP (f, 1, 8, "EDP");
    Series currPERF (f, 1, 9, "PERF");
    p.plotPowerLog({powerCap, currPower, smaPower/*, currENG, currEDP, currPERF*/});
    p.submitPlot();
}

void GpuEco::waitForGpuComputeActivity(int& status, int samplingPeriodInMilliSec = 1000)
{
    unsigned cyclesWithGpuActivity = 0;
    while (status)
    {
        usleep(samplingPeriodInMilliSec * 1000);
        deviceState_->sample();
        auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(trigger_);
        // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), tmpResult);
        logger_.logPowerLogLine(*deviceState_, tmpResult);
        if (tmpResult.instructionsCount_ > 0)
        {
            cyclesWithGpuActivity++;
        }
        else if (tmpResult.instructionsCount_ == 0)
        {
            cyclesWithGpuActivity = 0;
        }
        if (cyclesWithGpuActivity > 5) // to be parametrized
        {
            break;
        }
    }
}

PowAndPerfResult GpuEco::getReferenceResult(const int referenceSampleTimeInMilliSec)
{
    auto&& referenceResult = checkPowerAndPerformance(cfg_.referenceRunMultiplier_* referenceSampleTimeInMilliSec * 1000);
    // *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
    // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), referenceResult, referenceResult);
    logger_.logPowerLogLine(*deviceState_, referenceResult);
    // *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
    return referenceResult;
}

// int GpuEco::runTunningPhaseLS(
//     int& status,
//     int samplingPeriodInMilliSec,
//     const PowAndPerfResult& referenceResult,
//     TargetMetric metric)
// {
//     auto currBestResult = referenceResult;
//     int limitInWatts = maxPowerLimit_;
//     while(status)
//     {
//         gpu_->setPowerLimitInMicroWatts(limitInWatts * 1e6);
//         usleep(samplingPeriodInMilliSec * 1000);
//         deviceState_->sample();
//         auto&& tmpResult = deviceState_->getCurrentPowerAndPerf();
//         logger_.logPowerLogLine(*deviceState_, tmpResult, referenceResult);
//         // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), tmpResult, referenceResult);
//         // TODO: currently logCurrentResult function MUST be run before any isRightBetter() call as it actually evaluates
//         //       the EDS metric. This dependency shall be removed.
//         if (currBestResult.isRightBetter(tmpResult, metric))
//         {
//             currBestResult = std::move(tmpResult);
//         }
//         if (limitInWatts == minPowerLimit_)
//         {
//             break;
//         }
//         else if (limitInWatts > minPowerLimit_)
//         {
//             limitInWatts -= 5;
//         }
//     }
//     // *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
//     return currBestResult.appliedPowerCapInWatts_;
// }

// int GpuEco::runTunningPhaseGSS(
//     int& status,
//     int samplingPeriodInMilliSec,
//     const PowAndPerfResult& referenceResult,
//     TargetMetric metric)
// {
//     int EPSILON = (maxPowerLimit_ - minPowerLimit_)/ 25;
//     std::cout << "EPSILON: " << EPSILON << "\n";
//     int a = minPowerLimit_ * 1000; // milli watts
//     int b = maxPowerLimit_ * 1000; // milli watts
//     float phi = GoldenSectionSearchAlgorithm::PHI;
//     int leftCandidateInMilliWatts = b - int(phi * (b - a));
//     int rightCandidateInMilliWatts = a + int(phi * (b - a));
//     logCurrentRangeGSS(a, leftCandidateInMilliWatts, rightCandidateInMilliWatts, b);
//     bool measureL = true;
//     bool measureR = true;
//     PowAndPerfResult tmp = referenceResult;
//     while ((b - a) / 1000 > EPSILON)
//     {
//         auto fL = tmp;
//         if (measureL)
//         {
//             gpu_->setPowerLimitInMicroWatts(leftCandidateInMilliWatts * 10e3);
//             fL = checkPowerAndPerformance(samplingPeriodInMilliSec * 1000);
//             // std::cout << "measure left metric value " << fL.getInstrPerSecond() << std::endl;
//             // *bout_  << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), fL, referenceResult);
//             logger_.logPowerLogLine(*deviceState_, fL, referenceResult);
//         }
//         auto fR = tmp;
//         if (measureR)
//         {
//             gpu_->setPowerLimitInMicroWatts(rightCandidateInMilliWatts * 10e3);
//             fR = checkPowerAndPerformance(samplingPeriodInMilliSec * 1000);
//             // std::cout << "measure right metric value" << fR.getEnergyPerInstr() << std::endl;
//             // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), fR, referenceResult);
//             logger_.logPowerLogLine(*deviceState_, fR, referenceResult);
//         }

//         if (!fL.isRightBetter(fR, metric)) {
//             // choose subrange [a, rightCandidateInMilliWatts]
//             b = rightCandidateInMilliWatts;
//             rightCandidateInMilliWatts = leftCandidateInMilliWatts;
//             tmp = fL;
//             measureR = false;
//             measureL = true;
//             leftCandidateInMilliWatts = b - int(phi * (b - a));
//         } else {
//             // choose subrange [leftCandidateInMilliWatts, b]
//             a = leftCandidateInMilliWatts;
//             leftCandidateInMilliWatts = rightCandidateInMilliWatts;
//             tmp = fR;
//             measureR = true;
//             measureL = false;
//             rightCandidateInMilliWatts = a + int(phi * (b - a));
//         }
//         logCurrentRangeGSS(a, leftCandidateInMilliWatts, rightCandidateInMilliWatts, b);
//     }
//     // when stop condition was met return the middle of the subrange found
//     // *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
//     return (a + b) / (2 * 1000);
// }

void GpuEco::executeWithPowercap(
    int& status,
    unsigned powercapInMicroWatts,
    int samplingPeriodInMilliSec,
    int childPID,
    const PowAndPerfResult& referenceResult)
{
    gpu_->setPowerLimitInMicroWatts(powercapInMicroWatts);
    while (status)
    {
        usleep(samplingPeriodInMilliSec * 1000);
        deviceState_->sample();
        auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(trigger_);
        logger_.logPowerLogLine(*deviceState_, tmpResult, referenceResult);
        // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), tmpResult, referenceResult);
        waitpid(childPID, &status, WNOHANG);
    }       
}

PowAndPerfResult GpuEco::checkPowerAndPerformance(int usPeriod)
{
    auto pause = cfg_.msPause_ * 1000;
    usleep(pause);
    deviceState_->sample();
    auto resultAccumulator = deviceState_->getCurrentPowerAndPerf(trigger_);
    // std::cout << "\n[INFO] Firstt data point " << resultAccumulator << std::endl;
    while (usPeriod > pause){
        usleep(pause);
        deviceState_->sample();
        // logPowerToFile();
        auto tmp = deviceState_->getCurrentPowerAndPerf(trigger_);
        // *bout_ << logCurrentGpuResultLine(deviceState_->getTimeSinceObjectCreation(), tmp);
        logger_.logPowerLogLine(*deviceState_, tmp);
        resultAccumulator += tmp;
        // std::cout << "[INFO] Single data point " << tmp << std::endl;
        usPeriod -= pause;
    }
    // std::cout << "[INFO] Finall data point " << resultAccumulator << std::endl;

    return resultAccumulator;
}