/**
 * @file shader/module.h
 * @brief Vulkan shader module helper functions.
 */
#pragma once

#include "../api.h"

#include "wyre/result.h" /* Result<T> */

namespace wyre::shader {

/**
 * @brief Try to load a shader module from a file.
 */
Result<vk::ShaderModule> from_file(vk::Device device, std::string_view path);

}  // namespace wyre::shader
