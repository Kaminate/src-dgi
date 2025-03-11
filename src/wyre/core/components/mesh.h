#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace wyre {

class Files;
struct Triangle;

/**
 * @brief Mesh component, a list of triangles to be rendered.
 */
struct Mesh {
    std::vector<glm::vec3> vertices {};
    std::vector<glm::vec3> normals {};
    std::vector<uint32_t> indices {};
    glm::vec3 material {};
    /* Number of triangles. */
    size_t tri_count = 0u;

    Mesh() = default;
    /** @brief Create a mesh from raw triangle data. */
    Mesh(const Triangle* triangles, const size_t triangle_count);
    /** @brief Load a GLTF model from disk. */
    Mesh(const Files& files, const std::string& path, const glm::vec3 mat = glm::vec3(-1.0f), const size_t mesh_idx = 0u);
};

}  // namespace wyre
