#pragma once

/**
 * @brief Graphics Device.
 * 
 * --- [definition] ---
 * class Device;
 * 
 * --- [functions] ---
 * Result<void> init();
 * void start_frame();
 * void end_frame();
 * Result<void> destroy();
 */

#if defined(_WIN32) || defined(_WIN64)
#include "wyre/platform/vulkan/device.h"
#else
#error "platform is not supported."
#endif
