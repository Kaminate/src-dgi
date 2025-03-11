#pragma once

#include <glm/glm.hpp> /* glm::mat4 */

namespace wyre {

class Window;

/**
 * @brief Camera component, a window through which one can view the scene.
 */
struct Camera {
    /* Field of view in degrees. */
    float fov = 90.0f;

    Camera() = default;
    /**
     * @param fov Field of view in degrees.
     */
    Camera(const float fov);
    
    /**
     * @brief Get a projection matrix representing this camera's frustum.
     * 
     * @warning Calling this function can be costly.
     */
    glm::mat4 get_projection(const Window& window, const float near, const float far) const;
};

}  // namespace wyre
