#pragma once

#include <glm/glm.hpp>
#include <glm/ext/quaternion_float.hpp>

namespace wyre {

/**
 * @brief Transform component, holds position, rotation, & scale information.
 */
struct Transform {
    glm::vec3 position{};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::quat rotation{};

    /**
     * @brief Get a model matrix representing this transform.
     * 
     * @warning Calling this function can be costly.
     */
    glm::mat4 get_model() const;
};

}  // namespace wyre
