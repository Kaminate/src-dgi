#include "debug.h"

#include "../api.h"
#include "../device.h"

namespace wyre::debug {

#if DEBUG

void begin_label(const Device& device, std::string_view label, glm::vec3 color) {
    vk::DebugUtilsLabelEXT labelExt{};
    memcpy(labelExt.color.data(), &color.r, sizeof(glm::vec3));
    labelExt.color[3] = 1.0f;
    labelExt.pLabelName = label.data();
    device.get_frame().gcb.beginDebugUtilsLabelEXT(&labelExt, device.dldi);
}

void end_label(const Device& device) { device.get_frame().gcb.endDebugUtilsLabelEXT(device.dldi); }

#else

void begin_label(const Device& device, std::string_view label, glm::vec3 color) {}

void end_label(const Device& device) {}

#endif

}  // namespace wyre::debug
