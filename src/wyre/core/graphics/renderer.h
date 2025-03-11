#pragma once

/**
 * @brief Graphics Renderer.
 * 
 * --- [definition] ---
 * class Renderer : wyre::System;
 */

#if defined(_WIN32) || defined(_WIN64)
#include "wyre/platform/vulkan/renderer.h"
#else
#error "platform is not supported."
#endif
