#include "aabb.h"

namespace wyre {

AABB::AABB(const float min, const float max) : min(min), max(max) {}

AABB::AABB(const glm::vec3 min, const glm::vec3 max) : min(min), max(max) {}

void AABB::grow(const glm::vec3 point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void AABB::grow(const AABB& aabb) {
    if (aabb.min.x != 1e30f) {
        grow(aabb.min);
        grow(aabb.max);
    }
}

float AABB::area() const {
    const glm::vec3 e = max - min;
    return e.x * e.x + e.y * e.y + e.z * e.z;
}

}

