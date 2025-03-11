/**
 * @file hardware/ext.h
 * @brief All vulkan extension/layer querying related code.
 */
#pragma once

#include <vector>

namespace vk {
struct ExtensionProperties;
struct LayerProperties;
}

namespace wyre {

/* Vulkan device extensions */
extern const std::vector<const char*> DEVICE_EXT;

/**
 * @brief Validates a list of required extensions, comparing it with the available ones.
 */
bool validate_extensions(const std::vector<const char*>& required, const std::vector<vk::ExtensionProperties>& available);

/**
 * @brief Validates a list of validation layers, comparing it with the available ones.
 */
bool validate_layers(const std::vector<const char*>& required, const std::vector<vk::LayerProperties>& available);

/**
 * @brief Get the optimal validation layer extensions.
 */
std::vector<const char*> get_optimal_validation_layers(const std::vector<vk::LayerProperties>& supported_instance_layers);

/**
 * @brief Get the sdl extensions.
 */
std::vector<const char*> get_sdl_extensions();

}  // namespace wyre
