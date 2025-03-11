/**
 * @file hardware/device.cpp
 * @brief Vulkan device querying functions.
 */
#include "device.h"

namespace wyre {

/**
 * @brief Rank a physical device by its type.
 */
inline int rank_device_type(vk::PhysicalDevice device) {
    const vk::PhysicalDeviceType type = device.getProperties().deviceType;
    switch (type) {
        case vk::PhysicalDeviceType::eOther: return 0;
        case vk::PhysicalDeviceType::eCpu: return 1;
        case vk::PhysicalDeviceType::eIntegratedGpu: return 2; // return 2
        case vk::PhysicalDeviceType::eVirtualGpu: return 3;
        case vk::PhysicalDeviceType::eDiscreteGpu: return 4;
        default: return -1;
    }
}

/**
 * @brief Find the "best" physical device to use.
 */
Result<vk::PhysicalDevice> get_physical_device(vk::Instance instance) {
    /* Get a list of available physical devices */
    const vk::ResultValue result = instance.enumeratePhysicalDevices();
    if (result.result != vk::Result::eSuccess) return Err("failed to enumerate physical devices.");
    std::vector<vk::PhysicalDevice> physical_devices = result.value;

    if (physical_devices.empty()) {
        return Err("didn't find any physical devices.");
    }

    /* Select the "best" physical device */
    vk::PhysicalDevice phy_device = physical_devices.front();
    for (vk::PhysicalDevice candidate_device : physical_devices) {
        /* Get some info on the physical devices */
        const int type = rank_device_type(phy_device);
        const int c_type = rank_device_type(candidate_device);

        /* If the candidate device ranks higher, it becomes the newly selected device */
        if (c_type > type) {
            phy_device = candidate_device;
        }
    }
    return Ok(phy_device);
}

}  // namespace wyre
