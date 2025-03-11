#pragma once

#include "aabb.h"

namespace wyre {

/**
 * @brief A ray trace-able triangle.
 */
struct Triangle {
    glm::vec3 v0 {}; float r = 1.0f;
    glm::vec3 v1 {}; float g = 1.0f;
    glm::vec3 v2 {}; float b = 1.0f;

    Triangle() = default;
    Triangle(const glm::vec3 v0, const glm::vec3 v1, const glm::vec3 v2, const glm::vec3 color = glm::vec3(1.0f));

    AABB get_aabb() const;
    inline glm::vec3 get_centroid() const { return (v0 + v1 + v2) / 3.0f; };
};

struct Normals {
    glm::vec3 n0 {}; float _0 = 0.0f;
    glm::vec3 n1 {}; float _1 = 0.0f;
    glm::vec3 n2 {}; float _2 = 0.0f;
    
    Normals() = default;
    Normals(const glm::vec3 n0, const glm::vec3 n1, const glm::vec3 n2);
};

}  // namespace wyre
