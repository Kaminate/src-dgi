#include "triangle.h"

namespace wyre {

Triangle::Triangle(const glm::vec3 v0, const glm::vec3 v1, const glm::vec3 v2, const glm::vec3 color)
    : v0(v0), v1(v1), v2(v2), r(color.r), g(color.g), b(color.b) {}

AABB Triangle::get_aabb() const {
    AABB aabb{};
    aabb.grow(v0);
    aabb.grow(v1);
    aabb.grow(v2);
    return aabb;
}

Normals::Normals(const glm::vec3 n0, const glm::vec3 n1, const glm::vec3 n2) : n0(n0), n1(n1), n2(n2) {}

}  // namespace wyre
