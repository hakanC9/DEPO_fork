/*
   Copyright 2025, Intel Corporation

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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

struct MetricResult
{
    uint64_t inst_alu0 = 0;
    uint64_t inst_alu1 = 0;
    uint64_t inst_xmx  = 0;
    uint64_t inst_send = 0;
    uint64_t inst_ctrl = 0;
};

enum CollectorState
{
    COLLECTOR_STATE_IDLE     = 0,
    COLLECTOR_STATE_ENABLED  = 1,
    COLLECTOR_STATE_DISABLED = 2
};

class ZeMetricCollector
{
public:
    static ZeMetricCollector* Create(ze_driver_handle_t driver, ze_device_handle_t device, std::string group_name)
    {
        assert(driver != nullptr);
        assert(device != nullptr);

        // TODO: put somewhere information that we need sufficient privileges:
        // sudo sh -c 'echo 0 > /proc/sys/dev/i915/perf_stream_paranoid'

        // TODO: put somewhere information that metrics discovery library should
        // be available in LD_LIBRARY_PATH

        // create metric group
        zet_metric_group_handle_t group       = nullptr;
        ze_result_t               status      = ZE_RESULT_SUCCESS;
        uint32_t                  group_count = 0;
        status                                = zetMetricGroupGet(device, &group_count, nullptr);
        assert(status == ZE_RESULT_SUCCESS);
        if (group_count != 0)
        {
            std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
            status = zetMetricGroupGet(device, &group_count, group_list.data());
            assert(status == ZE_RESULT_SUCCESS);
            zet_metric_group_handle_t target = nullptr;
            for (uint32_t i = 0; i < group_count; ++i)
            {
                zet_metric_group_properties_t group_props {};
                group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                status            = zetMetricGroupGetProperties(group_list[i], &group_props);
                assert(status == ZE_RESULT_SUCCESS);

                if (group_name == group_props.name &&
                    (group_props.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED))
                {
                    target = group_list[i];
                    break;
                }
            }
            group = target;
        }
        if (group == nullptr)
        {
            std::cerr << "[WARNING] Unable to find target metric group: " << group_name << std::endl;
            return nullptr;
        }

        // create context
        status                           = ZE_RESULT_SUCCESS;
        ze_context_handle_t context      = nullptr;
        ze_context_desc_t   context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        status                           = zeContextCreate(driver, &context_desc, &context);
        assert(status == ZE_RESULT_SUCCESS);

        return new ZeMetricCollector(device, context, group);
    }

    ~ZeMetricCollector()
    {
        ze_result_t status = ZE_RESULT_SUCCESS;
        assert(collector_state_ == COLLECTOR_STATE_DISABLED);

        assert(context_ != nullptr);
        status = zeContextDestroy(context_);
        assert(status == ZE_RESULT_SUCCESS);
    }

    void DisableCollection() { DisableMetrics(); }

    void resetAccumulatedMetrics()
    {
        accumulated_metrics_                               = 0;
        std::vector<MetricResult>* metric_results_to_reset = nullptr;
        {
            std::lock_guard<std::mutex> lock(metric_results_mutex_);
            metric_results_to_reset = current_metric_results_;
            current_metric_results_ =
                (current_metric_results_ == &metric_results1_) ? &metric_results2_ : &metric_results1_;
        }
        metric_results_to_reset->clear();
    }

    uint64_t getAccumulatedMetricsSinceLastReset()
    {
        std::vector<MetricResult>* metric_results_to_process = nullptr;
        {
            std::lock_guard<std::mutex> lock(metric_results_mutex_);
            metric_results_to_process = current_metric_results_;
            current_metric_results_ =
                (current_metric_results_ == &metric_results1_) ? &metric_results2_ : &metric_results1_;
        }

        uint64_t acc_sum = 0;
        for (const auto& metric : *metric_results_to_process)
        {
            acc_sum += metric.inst_alu0 + metric.inst_alu1 + metric.inst_xmx + metric.inst_send + metric.inst_ctrl;
        }

        metric_results_to_process->clear();

        accumulated_metrics_ += (acc_sum / 1000000UL);
        return accumulated_metrics_;
    }

private:
    ZeMetricCollector(ze_device_handle_t device, ze_context_handle_t context, zet_metric_group_handle_t group)
    : device_(device), context_(context), metric_group_(group)
    {
        assert(device_ != nullptr);
        assert(context_ != nullptr);
        assert(metric_group_ != nullptr);
        SetCollectionConfig();
        SetReportSize();
        SetMetricIndices();
        EnableMetrics();
    }

    void EnableMetrics()
    {
        assert(collector_thread_ == nullptr);
        assert(collector_state_ == COLLECTOR_STATE_IDLE);

        collector_state_.store(COLLECTOR_STATE_IDLE, std::memory_order_release);
        collector_thread_ = new std::thread(Collect, this);
        assert(collector_thread_ != nullptr);

        while (collector_state_.load(std::memory_order_acquire) != COLLECTOR_STATE_ENABLED)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void DisableMetrics()
    {
        assert(collector_thread_ != nullptr);
        collector_state_.store(COLLECTOR_STATE_DISABLED, std::memory_order_release);
        collector_thread_->join();
        delete collector_thread_;
    }

    void SetCollectionConfig()
    {
        const auto cni = std::getenv("COLLECTOR_NOTIFY_INTERVAL");
        if (cni != nullptr)
            collector_notify_interval = std::stoul(cni);
        const auto csp = std::getenv("COLLECTOR_SAMPLING_PERIOD_NS");
        if (csp != nullptr)
            collector_sampling_period_ns = std::stoul(csp);
        const auto cd = std::getenv("COLLCETOR_DELAY_NS");
        if (cd != nullptr)
            collector_delay_ns = std::stoull(cd);
        std::cout << "collector_notify_interval: " << collector_notify_interval << "\n";
        std::cout << "collector_sampling_period_ns: " << collector_sampling_period_ns << "\n";
        std::cout << "collector_delay_ns: " << collector_delay_ns << std::endl;
    }

    void SetReportSize()
    {
        assert(metric_group_ != nullptr);
        ze_result_t status = ZE_RESULT_SUCCESS;

        zet_metric_group_properties_t group_props {};
        group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
        status            = zetMetricGroupGetProperties(metric_group_, &group_props);
        assert(status == ZE_RESULT_SUCCESS);
        report_size_ = group_props.metricCount;
    }

    void SetMetricIndices()
    {
        assert(metric_group_ != nullptr);

        auto GetMetricId = [&](zet_metric_group_handle_t group, const std::string& name) -> int
        {
            assert(group != nullptr);

            ze_result_t status       = ZE_RESULT_SUCCESS;
            uint32_t    metric_count = 0;
            status                   = zetMetricGet(group, &metric_count, nullptr);
            assert(status == ZE_RESULT_SUCCESS);

            if (metric_count == 0)
            {
                return -1;
            }

            std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
            status = zetMetricGet(group, &metric_count, metric_list.data());
            assert(status == ZE_RESULT_SUCCESS);

            int target = -1;
            for (uint32_t i = 0; i < metric_count; ++i)
            {
                zet_metric_properties_t metric_props {};
                status = zetMetricGetProperties(metric_list[i], &metric_props);
                assert(status == ZE_RESULT_SUCCESS);

                if (name == metric_props.name)
                {
                    target = i;
                    break;
                }
            }

            return target;
        };

        inst_alu0_id_ = GetMetricId(metric_group_, "XVE_INST_EXECUTED_ALU0_ALL");
        assert(inst_alu0_id_ > 0);
        inst_alu1_id_ = GetMetricId(metric_group_, "XVE_INST_EXECUTED_ALU1_ALL");
        assert(inst_alu1_id_ > 0);
        inst_xmx_id_ = GetMetricId(metric_group_, "XVE_INST_EXECUTED_XMX_ALL");
        assert(inst_xmx_id_ > 0);
        inst_send_id_ = GetMetricId(metric_group_, "XVE_INST_EXECUTED_SEND_ALL");
        assert(inst_send_id_ > 0);
        inst_ctrl_id_ = GetMetricId(metric_group_, "XVE_INST_EXECUTED_CONTROL_ALL");
        assert(inst_ctrl_id_ > 0);
    }

    void AppendCalculatedMetrics(const std::vector<uint8_t>& storage)
    {
        assert(storage.size() > 0);

        ze_result_t                    status = ZE_RESULT_SUCCESS;
        std::vector<zet_typed_value_t> report_list;
        assert(metric_group_ != nullptr);

        if (storage.size() == 0)
        {
            return;
        }

        uint32_t value_count = 0;
        status               = zetMetricGroupCalculateMetricValues(metric_group_,
                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                     storage.size(),
                                                     storage.data(),
                                                     &value_count,
                                                     nullptr);
        assert(status == ZE_RESULT_SUCCESS);
        assert(value_count > 0);

        report_list.resize(value_count);
        status = zetMetricGroupCalculateMetricValues(metric_group_,
                                                     ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                                                     storage.size(),
                                                     storage.data(),
                                                     &value_count,
                                                     report_list.data());
        assert(status == ZE_RESULT_SUCCESS);
        report_list.resize(value_count);

        MetricResult metric_result;

        const zet_typed_value_t* report = report_list.data();
        while (report < report_list.data() + report_list.size())
        {
            assert(report[inst_alu0_id_].type == ZET_VALUE_TYPE_UINT64);
            uint64_t inst_alu0 = report[inst_alu0_id_].value.ui64;
            assert(report[inst_alu1_id_].type == ZET_VALUE_TYPE_UINT64);
            uint64_t inst_alu1 = report[inst_alu1_id_].value.ui64;
            assert(report[inst_xmx_id_].type == ZET_VALUE_TYPE_UINT64);
            uint64_t inst_xmx = report[inst_xmx_id_].value.ui64;
            assert(report[inst_send_id_].type == ZET_VALUE_TYPE_UINT64);
            uint64_t inst_send = report[inst_send_id_].value.ui64;
            assert(report[inst_ctrl_id_].type == ZET_VALUE_TYPE_UINT64);
            uint64_t inst_ctrl = report[inst_ctrl_id_].value.ui64;

            metric_result.inst_alu0 += inst_alu0;
            metric_result.inst_alu1 += inst_alu1;
            metric_result.inst_xmx += inst_xmx;
            metric_result.inst_send += inst_send;
            metric_result.inst_ctrl += inst_ctrl;

            report += report_size_;
        }

        {
            std::lock_guard<std::mutex> lock(metric_results_mutex_);
            current_metric_results_->push_back(metric_result);
        }
    }

    static void Collect(ZeMetricCollector* collector)
    {
        assert(collector != nullptr);

        assert(collector->context_ != nullptr);
        assert(collector->device_ != nullptr);
        assert(collector->metric_group_ != nullptr);

        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zetContextActivateMetricGroups(collector->context_, collector->device_, 1, &collector->metric_group_);
        assert(status == ZE_RESULT_SUCCESS);

        ze_event_pool_desc_t   event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
                                                  nullptr,
                                                  ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                                  1};
        ze_event_pool_handle_t event_pool      = nullptr;
        status = zeEventPoolCreate(collector->context_, &event_pool_desc, 0, nullptr, &event_pool);
        assert(status == ZE_RESULT_SUCCESS);

        ze_event_desc_t   event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC,
                                        nullptr,
                                        0,
                                        ZE_EVENT_SCOPE_FLAG_HOST,
                                        ZE_EVENT_SCOPE_FLAG_HOST};
        ze_event_handle_t event      = nullptr;
        status                       = zeEventCreate(event_pool, &event_desc, &event);
        assert(status == ZE_RESULT_SUCCESS);

        zet_metric_streamer_desc_t   metric_streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC,
                                                             nullptr,
                                                             collector->collector_notify_interval,
                                                             collector->collector_sampling_period_ns};
        zet_metric_streamer_handle_t metric_streamer      = nullptr;
        status                                            = zetMetricStreamerOpen(collector->context_,
                                       collector->device_,
                                       collector->metric_group_,
                                       &metric_streamer_desc,
                                       event,
                                       &metric_streamer);
        assert(status == ZE_RESULT_SUCCESS);

        collector->collector_state_.store(COLLECTOR_STATE_ENABLED, std::memory_order_release);

        std::vector<uint8_t> storage;
        while (collector->collector_state_.load(std::memory_order_acquire) != COLLECTOR_STATE_DISABLED)
        {
            status = zeEventHostSynchronize(event, collector->collector_delay_ns);
            assert(status == ZE_RESULT_SUCCESS || status == ZE_RESULT_NOT_READY);

            size_t data_size = 0;
            status           = zetMetricStreamerReadData(metric_streamer, UINT32_MAX, &data_size, nullptr);
            assert(status == ZE_RESULT_SUCCESS);

            if (data_size > 0)
            {
                storage.resize(data_size);
                status = zetMetricStreamerReadData(metric_streamer, UINT32_MAX, &data_size, storage.data());
                assert(status == ZE_RESULT_SUCCESS);
                storage.resize(data_size);
                if (storage.size() > 0)
                    collector->AppendCalculatedMetrics(storage);
            }
        }

        status = zetMetricStreamerClose(metric_streamer);
        assert(status == ZE_RESULT_SUCCESS);

        status = zeEventDestroy(event);
        assert(status == ZE_RESULT_SUCCESS);
        status = zeEventPoolDestroy(event_pool);
        assert(status == ZE_RESULT_SUCCESS);

        status = zetContextActivateMetricGroups(collector->context_, collector->device_, 0, nullptr);
        assert(status == ZE_RESULT_SUCCESS);
    }

private:
    ze_device_handle_t  device_  = nullptr;
    ze_context_handle_t context_ = nullptr;

    std::thread*                collector_thread_ = nullptr;
    std::atomic<CollectorState> collector_state_ {COLLECTOR_STATE_IDLE};

    zet_metric_group_handle_t metric_group_ = nullptr;
    // double buffering
    std::vector<MetricResult>  metric_results1_;
    std::vector<MetricResult>  metric_results2_;
    std::vector<MetricResult>* current_metric_results_ = &metric_results1_;
    std::mutex                 metric_results_mutex_;

    uint64_t accumulated_metrics_ = 0;
    uint32_t report_size_;
    int      inst_alu0_id_;
    int      inst_alu1_id_;
    int      inst_xmx_id_;
    int      inst_send_id_;
    int      inst_ctrl_id_;

public:
    uint32_t collector_notify_interval    = 32768;
    uint32_t collector_sampling_period_ns = 5000000;
    uint64_t collector_delay_ns           = 50000000;
};