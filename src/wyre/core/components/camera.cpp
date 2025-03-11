#include "camera.h"

#include <glm/ext/matrix_clip_space.hpp> /* glm::perspective */

#include "wyre/core/system/window.h"

namespace wyre {

Camera::Camera(const float fov) : fov(fov) {}

glm::mat4 Camera::get_projection(const Window& window, const float near, const float far) const {
    const float aspect = (float)window.width / (float)window.height;
    return glm::perspective(glm::radians(fov), aspect, near, far);
}

}  // namespace wyre
