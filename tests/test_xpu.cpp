#include "eco.hpp"
#include "devices/xpu_device.hpp"
#include "../src/logging.hpp"
#include <cstdio>
#include <unordered_map>
#include <tuple>
#include <cstring>
#include <type_traits>

#define CHECK(x)                                                                                                       \
    if (x != true)                                                                                                     \
    {                                                                                                                  \
        exit(-1);                                                                                                      \
    }

std::unordered_map<std::string, std::tuple<unsigned, unsigned>> known_devices_power;
std::unordered_map<std::string, std::tuple<unsigned, unsigned>> known_devices_current;

bool test_init_eco_xpu_device(int devID)
{
    std::shared_ptr<Device> xpu_device = std::make_unique<XPUDevice>(devID);
    auto                    eco        = std::make_unique<Eco>(xpu_device);

    return true;
}

bool test_xpu_min_max_limits(int devID, bool useAmperes)
{
    auto xpu_device = std::make_unique<XPUDevice>(devID, useAmperes);
    auto limits     = xpu_device->getMinMaxLimitInWatts();


    std::unordered_map<std::string, std::tuple<unsigned, unsigned>>& known_devices =
        useAmperes ? known_devices_current : known_devices_power;

    unsigned expectedMinLimit;
    unsigned expectedMaxLimit;

    auto key = xpu_device->getName();
    if (known_devices.find(key) == known_devices.end())
    {
        LOG_ERROR("Unsupported device: {}", key);
        return false;
    }
    std::tie(expectedMinLimit, expectedMaxLimit) = known_devices[key];

    if ((limits.first != expectedMinLimit) || (limits.second != expectedMaxLimit))
    {
        LOG_ERROR("Error: expected power limits differ from actual read ones");
        return false;
    }

    unsigned expectedMiddleLimit = (expectedMinLimit + expectedMaxLimit) / 2;

    xpu_device->setPowerLimitInMicroWatts(expectedMiddleLimit * 1000000);

    auto currentLimit = xpu_device->getPowerLimitInWatts();

    // Restore max limit
    xpu_device->setPowerLimitInMicroWatts(expectedMaxLimit * 1000000);
    if (currentLimit != expectedMiddleLimit)
    {
        LOG_ERROR("Expected max power limit({}) differ from actually read({})", expectedMiddleLimit, limits.second);
        return false;
    }

    return true;
}

bool test_xpu_power_limit_resetting(int devID) {

    // Create XPU device and set power limit to half of its max value
    auto xpu_device = std::make_unique<XPUDevice>(devID, false);
    auto limits     = xpu_device->getMinMaxLimitInWatts();

    unsigned currentMinLimit;
    unsigned currentMaxLimit;

    auto key = xpu_device->getName();
    if (known_devices_power.find(key) == known_devices_power.end())
    {
        LOG_ERROR("Unsupported device: {}", key);
        return false;
    }
    std::tie(currentMinLimit, currentMaxLimit) = known_devices_power[key];

    unsigned currentMiddleLimit = (currentMinLimit + currentMaxLimit) / 2;

    xpu_device->setPowerLimitInMicroWatts(currentMiddleLimit * 1000000);

    auto currentLimit = xpu_device->getPowerLimitInWatts();

    if (currentLimit != currentMiddleLimit)
    {
        LOG_ERROR("Expected power limit({}) differ from actually read({})", currentMiddleLimit, currentLimit);
        return false;
    }
    // After setting sustained power limit we will create now xpu_device with power limit and already set sustained power limit should be reverted to max available value
    xpu_device = std::make_unique<XPUDevice>(devID, true);
    auto sustainedLimit = xpu_device->getPowerLimitSustained();

    if (sustainedLimit != currentMaxLimit)
    {
        LOG_ERROR("Expected sustained power limit({}) differ from actually read({})", currentMaxLimit, sustainedLimit);
        return false;
    }
    return true;
}

void init_devices_data()
{
    known_devices_power["Intel(R) Data Center GPU Max 1100"]   = std::make_tuple(150, 300);
    known_devices_current["Intel(R) Data Center GPU Max 1100"] = std::make_tuple(14, 48);
}

int main()
{
    LOAD_ENV_LEVELS()
    init_devices_data();

    CHECK(test_init_eco_xpu_device(0));
    CHECK(test_xpu_min_max_limits(0, false));  // Power limit: sustained (watts)
    CHECK(test_xpu_min_max_limits(0, true));   // Power limit: peak (amperes)
    CHECK(test_xpu_power_limit_resetting(0));

    return 0;
}
