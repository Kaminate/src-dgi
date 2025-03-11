#pragma once

#include <glm/glm.hpp>

namespace wyre {

struct AABB {
    glm::vec3 min = glm::vec3(1e30f); 
    glm::vec3 max = glm::vec3(-1e30f);

    AABB() = default;
    AABB(const float min, const float max);
    AABB(const glm::vec3 min, const glm::vec3 max);

    /** @brief Grow AABB to include a given point. */
    void grow(const glm::vec3 point);
    /** @brief Grow AABB to include a given AABB. */
    void grow(const AABB& aabb);
    /** @returns The area of the AABB. */
    float area() const;
};

}  // namespace wyre
