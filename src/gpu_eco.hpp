#pragma once

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include "Eco.hpp"
#include "plot_builder.hpp"
#include "helpers/params_config.hpp"
#include "helpers/both_stream.hpp"
#include "helpers/power_and_perf_result.hpp"
#include "helpers/log.hpp"

#include <cuda.h>
#include <nvml.h>

static inline
void logCurrentRangeGSS(int a, int leftCandidate, int rightCandidate, int b) {
    std::cout << "#--------------------------------\n"
              << "# Current GSS range: |"
              << a << " "
              << leftCandidate << " "
              << rightCandidate << " "
              << b << "|\n"
              << "#--------------------------------\n";
}

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

std::string generateUniqueDir() {
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


int long long readValueFromFile (std::string fileName) {
    std::ifstream limitFile (fileName.c_str());
    std::string line;
    long long int limit = -1;
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

class CudaDevice // this class should be named "cuda device container or sth like that as it stores all the devices"
{
  public:
    CudaDevice(int devID=0) :
     deviceID_(devID) // and then this field shall not be a member of this class as the API allows for access to any device
    {
        int major;
        CUresult result;
        CUdevice device{deviceID_};

        result = cuInit(0);
        if (result != CUDA_SUCCESS) {
            printf("Error code %d on cuInit\n", result);
            exit(-1);
        }
        result = cuDeviceGet(&device,0);
        if (result != CUDA_SUCCESS) {
            printf("Error code %d on cuDeviceGet\n", result);
            exit(-1);
        }

        result = cuDeviceGetAttribute (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
        if (result != CUDA_SUCCESS) {
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
    }
    int getPowerLimit(unsigned deviceID)
    {
        unsigned currPowerLimitInMilliWatts = 0;
        nvmlReturn_t nvResult = nvmlDeviceGetEnforcedPowerLimit (deviceHandles_[deviceID], &currPowerLimitInMilliWatts);
        if (NVML_SUCCESS != nvResult) {
            printf("Failed to GET current power limit: %s\n", nvmlErrorString(nvResult));
            return -1;
        }
        return currPowerLimitInMilliWatts;
    }

    std::pair<unsigned, unsigned> getMinMaxLimitInWatts(unsigned deviceID)
    {
        unsigned min = 0, max = 0;
        nvmlReturn_t nvResult = nvmlDeviceGetPowerManagementLimitConstraints (deviceHandles_[deviceID], &min, &max);
        if (NVML_SUCCESS != nvResult) {
            printf("Failed to GET min/max power limit: %s\n", nvmlErrorString(nvResult));
        }
        return std::make_pair(min/1000, max/1000);
    }

    void setPowerLimit(unsigned deviceID, int limitInWatts)
    {
        nvmlReturn_t nvResult = nvmlDeviceSetPowerManagementLimit (deviceHandles_[deviceID], limitInWatts * 1000);
        if (NVML_SUCCESS != nvResult) {
            printf("Failed to SET current power limit %d: %s\n", limitInWatts * 1000, nvmlErrorString(nvResult));
            return;
        }
    }

    void resetKernelCounterRegister()
    {
        std::ofstream kernelCounterFile;
        kernelCounterFile.open("kernels_count", std::ios::out | std::ios::trunc);
        kernelCounterFile << "0";
        kernelCounterFile.close();
    }

    double getCurrentPowerInWattsForDeviceID() // this method shall have the input parameter "deviceID" back
    {
        unsigned power;
        nvmlReturn_t nvResult = nvmlDeviceGetPowerUsage(deviceHandles_[deviceID_], &power);
        if (NVML_SUCCESS != nvResult) {
            printf("Failed to get power usage: %s\n", nvmlErrorString(nvResult));
            return -1.0;
        }
        return (double)power/1000.0;
    }

  private:
    void initDeviceHandles()
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
    nvmlReturn_t nvResult_;
    unsigned int deviceCount_ {0};
    int deviceID_;
    std::vector<nvmlDevice_t> deviceHandles_;
};

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

struct NvidiaState
{
    NvidiaState() = delete;
    NvidiaState(double pow, unsigned long long ker, TimePoint t) :
        power_(pow), kernelsCount_(ker), time_(t)
    {
    }
    double power_;
    unsigned long long kernelsCount_;
    TimePoint time_;
};

class GpuDeviceState
{
  public:
    GpuDeviceState(std::shared_ptr<CudaDevice>& device) :
        absoluteStartTime_(std::chrono::high_resolution_clock::now()),
        timeOfLastReset_(std::chrono::high_resolution_clock::now()),
        gpu_(device),
        prev_(0.0, 0, timeOfLastReset_),
        curr_(prev_),
        next_(prev_)
    {
        // prev_ = curr_ = next_ = NvidiaState(0.0, 0,);
        sample();
        sample();
    }

    GpuDeviceState& sample()
    {
        prev_ = curr_;
        curr_ = next_;

        // -----------------------------------------------
        // Below shall be enabled when DEPO upgrade
        // for GPU is merged
        // -----------------------------------------------
        // long long int kernelsCountTmp {-1};
        // do
        // {
        //     kernelsCountTmp = readValueFromFile("./kernels_count");
        // }
        // while (kernelsCountTmp == -1);

        // next_ = NvidiaState(
        //     gpu_->getCurrentPowerInWattsForDeviceID(),
        //     (unsigned long long)kernelsCountTmp,
        //     std::chrono::high_resolution_clock::now());
        // -----------------------------------------------
        next_ = NvidiaState(
            gpu_->getCurrentPowerInWattsForDeviceID(),
            0, // kernels count for future use
            std::chrono::high_resolution_clock::now());
        // -----------------------------------------------

        auto timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(next_.time_ - curr_.time_).count();
        totalEnergySinceReset_ += next_.power_ * timeDelta / 1000;
        return *this;
    }

    PowAndPerfResult getCurrentPowerAndPerf(int deviceID) const
    {
        double timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(next_.time_ - curr_.time_).count();
        return PowAndPerfResult(
            (double)(next_.kernelsCount_ - curr_.kernelsCount_),
            timeDelta / 1000,
            gpu_->getPowerLimit(deviceID),
            next_.power_ * timeDelta / 1000, // Watts x seconds
            next_.power_,
            next_.power_ // TODO: this should be filtered power
            );
    }

    template <class Resolution = std::chrono::milliseconds>
    double getTimeSinceReset() const
    {
        return std::chrono::duration_cast<Resolution>(
                    std::chrono::high_resolution_clock::now()  - timeOfLastReset_).count();
    }

    template <class Resolution = std::chrono::milliseconds>
    double getAbsoluteTimeSinceStart() const
    {
        return std::chrono::duration_cast<Resolution>(
                    std::chrono::high_resolution_clock::now()  - absoluteStartTime_).count();
    }

    double getEnergySinceReset() const
    {
        return totalEnergySinceReset_;
    }

    void resetState()
    {
        timeOfLastReset_ = std::chrono::high_resolution_clock::now();
        totalEnergySinceReset_ = 0.0;
        gpu_->resetKernelCounterRegister();
        sample();
        sample();
    }

  private:
    TimePoint absoluteStartTime_;
    TimePoint timeOfLastReset_;
    std::shared_ptr<CudaDevice> gpu_;
    NvidiaState prev_, curr_, next_;
    double totalEnergySinceReset_ {0.0};
};

class GpuEco : public EcoApi
{
  public:
    GpuEco(int deviceID = 0) : deviceID_(deviceID)
    {
        gpu_ = std::make_shared<CudaDevice>(deviceID);
        deviceState_ = std::make_unique<GpuDeviceState>(gpu_);
        std::tie(minPowerLimit_, maxPowerLimit_) = gpu_->getMinMaxLimitInWatts(deviceID_);
        defaultPowerLimit_ = gpu_->getPowerLimit(deviceID_) / 1000;
        gpu_->resetKernelCounterRegister();
        const auto dir = generateUniqueDir();
        outPowerFileName_ = dir + "power_log.csv";
        outResultFileName_ = dir + "result.csv";
        outPowerFile_.open(outPowerFileName_, std::ios::out | std::ios::trunc);
        bout_ = std::make_unique<BothStream>(outPowerFile_);
    }

    virtual ~GpuEco()
    {
        gpu_->setPowerLimit(deviceID_, defaultPowerLimit_);
        outPowerFile_.close();
    }

    void idleSample(int milliseconds) override
    {
        usleep(milliseconds * 1000);
        deviceState_->sample();
        auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(deviceID_);
        *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), tmpResult, tmpResult);
    }

    FinalPowerAndPerfResult runAppWithSampling(char* const* argv, int argc) override
    {
        std::string command;
        for (int i=1; i < argc; i++)
        {
            command += argv[i];
            command += " ";
        }
        deviceState_->resetState();
        int status;
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
            gpu_->getPowerLimit(deviceID_) / 1000,
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

    void staticEnergyProfiler(char* const* argv, int argc, BothStream& stream)
    {
        std::vector<FinalPowerAndPerfResult> oneSeriesResultVec;
        const auto&& warmup = runAppWithSampling(argv, argc);
        stream << "# " << std::fixed << std::setprecision(3) << warmup << "\n";
        stream << "# warmup done #\n";
        //
        FinalPowerAndPerfResult reference;
        for(auto i = 0; i < cfg_.numIterations_; i++) {
            auto tmp = runAppWithSampling(argv, argc);
            reference += tmp;
            stream << "# " << std::fixed << std::setprecision(3) << tmp << "\n";
        }
        reference /= cfg_.numIterations_;
        //
        oneSeriesResultVec.push_back(reference);
        stream << reference << "\n";
        for (unsigned cap = maxPowerLimit_; cap >= minPowerLimit_; cap-=5)
        {
            gpu_->setPowerLimit(deviceID_, cap);
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
            oneSeriesResultVec.emplace_back(gpu_->getPowerLimit(deviceID_) / 1000,
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

    FinalPowerAndPerfResult runAppWithSearch(
        char* const* argv,
        TargetMetric metric,
        SearchType searchType,
        int argc) override
    {
        std::string command;
        for (int i=1; i < argc; i++)
        {
            command += argv[i];
            command += " ";
        }
        deviceState_->resetState();
        int status = 1;
        double waitTime = 0.0, testTime = 0.0;
        int bestResultCap = 300;
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
                waitpid(childProcId, &status, WNOHANG);
                waitTime = measureDuration([&, this] {
                    waitForGpuComputeActivity(status, cfg_.msPause_);
                });
                PowAndPerfResult referenceRun;
                testTime = measureDuration([&, this] {

                    referenceRun = getReferenceResult(cfg_.msTestPhasePeriod_);

                    if (searchType == SearchType::LINEAR_SEARCH)
                    {
                        bestResultCap = runTunningPhaseLS(status, cfg_.msTestPhasePeriod_, referenceRun, metric);
                    }
                    else
                    {
                        bestResultCap = runTunningPhaseGSS(status, cfg_.msTestPhasePeriod_, referenceRun, metric);
                    }
                });
                executeWithPowercap(status, bestResultCap, cfg_.msPause_, childProcId, referenceRun);
                gpu_->setPowerLimit(deviceID_, defaultPowerLimit_);
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
            bestResultCap,
            deviceState_->getEnergySinceReset(),
            deviceState_->getEnergySinceReset() / execTime,
            0.0,
            0.0,
            0.0,
            TimeResult(execTime, waitTime, testTime),
            0.0,
            0.0);
    }

    void plotPowerLog() override
    {
        bout_->flush();
        outPowerFile_.close();
        std::cout << "Processing " << outPowerFileName_ << " file...\n";
        std::string imgFileName = outPowerFileName_;
        PlotBuilder p(imgFileName.replace(imgFileName.end() - 4,
                                        imgFileName.end(),
                                        ".png"));
        p.setPlotTitle("Dynamic power capping for NVIDIA GPU");
        Series powerCap (outPowerFileName_, 1, 2, "P cap [W]");
        Series currPower (outPowerFileName_, 1, 3, "P[W]");
        Series currENG (outPowerFileName_, 1, 7, "ENG");
        Series currEDP (outPowerFileName_, 1, 8, "EDP");
        Series currPERF (outPowerFileName_, 1, 9, "PERF");

        p.plotPowerLog({powerCap, currPower/*, currENG, currEDP, currPERF*/});
        p.submitPlot();
    }

    void waitForGpuComputeActivity(int& status, int samplingPeriodInMilliSec = 1000)
    {
        unsigned cyclesWithGpuActivity = 0;
        while (status)
        {
            usleep(samplingPeriodInMilliSec * 1000);
            deviceState_->sample();
            auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(deviceID_);
            *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), tmpResult, tmpResult); // reference result is not relevant yet
            if (tmpResult.instr > 0)
            {
                cyclesWithGpuActivity++;
            }
            else if (tmpResult.instr == 0)
            {
                cyclesWithGpuActivity = 0;
            }
            if (cyclesWithGpuActivity > 5) // to be parametrized
            {
                break;
            }
        }
    }

    PowAndPerfResult getReferenceResult(const int referenceSampleTimeInMilliSec)
    {
        usleep(3 * referenceSampleTimeInMilliSec * 1000);
        auto&& referenceResult = deviceState_->sample().getCurrentPowerAndPerf(deviceID_);
        *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
        *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), referenceResult, referenceResult);
        *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
        return referenceResult;
    }

    int runTunningPhaseLS(
        int& status,
        int samplingPeriodInMilliSec,
        const PowAndPerfResult& referenceResult,
        TargetMetric metric)
    {
        auto currBestResult = referenceResult;
        int powercap = maxPowerLimit_;
        while(status)
        {
            gpu_->setPowerLimit(deviceID_, powercap);
            usleep(samplingPeriodInMilliSec * 1000);
            deviceState_->sample();
            auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(deviceID_);
            *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), tmpResult, referenceResult);
            // TODO: currently logCurrentResult function MUST be run before any isRightBetter() call as it actually evaluates
            //       the EDS metric. This dependency shall be removed.
            if (currBestResult.isRightBetter(tmpResult, metric))
            {
                currBestResult = std::move(tmpResult);
            }
            if (powercap == minPowerLimit_)
            {
                break;
            }
            else if (powercap > minPowerLimit_)
            {
                powercap -= 5;
            }
        }
        *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
        return currBestResult.pCap;
    }

    int runTunningPhaseGSS(
        int& status,
        int samplingPeriodInMilliSec,
        const PowAndPerfResult& referenceResult,
        TargetMetric metric)
    {
        int EPSILON = (maxPowerLimit_ - minPowerLimit_)/ 25;
        std::cout << "EPSILON: " << EPSILON << "\n";
        int a = minPowerLimit_ * 1000;
        int b = maxPowerLimit_ * 1000;
        float phi = (sqrt(5) - 1) / 2; // this is equal 0.618 and it is reverse of 1.618
        int leftCandidate = b - int(phi * (b - a));
        int rightCandidate = a + int(phi * (b - a));
        logCurrentRangeGSS(a, leftCandidate, rightCandidate, b);
        bool measureL = true;
        bool measureR = true;
        PowAndPerfResult tmp = referenceResult;
        while ((b - a) / 1000 > EPSILON) {
            auto fL = tmp;
            if (measureL)
            {
                gpu_->setPowerLimit(deviceID_, leftCandidate / 1000);
                usleep(samplingPeriodInMilliSec * 1000);
                deviceState_->sample();
                fL = deviceState_->getCurrentPowerAndPerf(deviceID_);
                // std::cout << "measure left metric value " << fL.getInstrPerSecond() << std::endl;
            }
            *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), fL, referenceResult);
            auto fR = tmp;
            if (measureR)
            {
                gpu_->setPowerLimit(deviceID_, rightCandidate / 1000);
                usleep(samplingPeriodInMilliSec * 1000);
                deviceState_->sample();
                fR = deviceState_->getCurrentPowerAndPerf(deviceID_);
                // std::cout << "measure right metric value" << fR.getEnergyPerInstr() << std::endl;
            }
            *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), fR, referenceResult);

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
        *bout_ << "#-----------------------------------------------------------------------------------------------------------------------------------\n";
        return (a + b) / (2 * 1000);
    }

    void executeWithPowercap(
        int& status,
        unsigned powercapInWatts,
        int samplingPeriodInMilliSec,
        int childPID,
        const PowAndPerfResult& referenceResult)
    {
        gpu_->setPowerLimit(deviceID_, powercapInWatts);
        while (status)
        {
            usleep(samplingPeriodInMilliSec * 1000);
            deviceState_->sample();
            auto&& tmpResult = deviceState_->getCurrentPowerAndPerf(deviceID_);
            *bout_ << logCurrentGpuResultLine(deviceState_->getAbsoluteTimeSinceStart(), tmpResult, referenceResult);
            waitpid(childPID, &status, WNOHANG);
        }       
    }

  private:
    std::shared_ptr<CudaDevice> gpu_;
    std::unique_ptr<GpuDeviceState> deviceState_;
    unsigned minPowerLimit_, maxPowerLimit_, defaultPowerLimit_;
    int deviceID_;
    std::ofstream outPowerFile_;
    std::unique_ptr<BothStream> bout_;
  public:
    std::string outPowerFileName_;
};
