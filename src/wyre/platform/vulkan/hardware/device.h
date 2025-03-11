/**
 * @file hardware/device.h
 * @brief Vulkan device querying functions.
 */
#pragma once

#include "../api.h"

#include "wyre/result.h" /* Result<T> */

namespace wyre {

/**
 * @brief Find the "best" physical device to use.
 */
Result<vk::PhysicalDevice> get_physical_device(vk::Instance instance);

}  // namespace wyre
