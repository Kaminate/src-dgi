#pragma once

#include <string>

#include <glm/glm.hpp>

namespace wyre {
class Device;
}

namespace wyre::debug {

/**
 * @brief Mark the start of a debug label in the command buffer.
 */
void begin_label(const Device& device, std::string_view label, glm::vec3 color);

/**
 * @brief Mark the end of a debug label in the command buffer.
 */
void end_label(const Device& device);

}  // namespace wyre::debug
