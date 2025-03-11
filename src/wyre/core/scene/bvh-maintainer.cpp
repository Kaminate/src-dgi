/**
 * @file pipelines/scene-bvh.cpp
 * @brief Scene BVH Maintainer.
 */
#include "bvh-maintainer.h"

#include "wyre/core/system/log.h"
#include "wyre/core/ecs.h"
#include "wyre/core/components/mesh.h"
#include "wyre/core/components/transform.h"

namespace wyre {

using namespace scene;

void SceneBvhMaintainer::maintain(ECS& ecs) {
    if (bvh.prims != nullptr) return;

    /* Create a group owning Mesh, which also gives us access to the Transform */
    const entt::basic_group mesh_group = ecs.registry.group<const Mesh>(entt::get<const Transform>);
    
    std::vector<Triangle> triangles{};
    std::vector<Normals> normals{};
    triangles.reserve(1024);
    normals.reserve(1024);

    /* Collect the triangles from all Mesh instances */
    for (auto&& [entity, mesh, transform] : mesh_group.each()) {
        const glm::mat4 model = transform.get_model();

        /* If there's no indices, just read the vertex array directly */
        if (mesh.indices.empty()) {
            for (size_t i = 0; i < mesh.tri_count; ++i) {
                const glm::vec3 v0 = model * glm::vec4(mesh.vertices[i * 3 + 0], 1.0f);
                const glm::vec3 v1 = model * glm::vec4(mesh.vertices[i * 3 + 1], 1.0f);
                const glm::vec3 v2 = model * glm::vec4(mesh.vertices[i * 3 + 2], 1.0f);
                triangles.emplace_back(v0, v1, v2, mesh.material);
                const glm::vec3 n0 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[i * 3 + 0], 0.0f)));
                const glm::vec3 n1 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[i * 3 + 1], 0.0f)));
                const glm::vec3 n2 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[i * 3 + 2], 0.0f)));
                normals.emplace_back(n0, n1, n2);
            }
            continue;
        }

        /* Use the indices array */
        for (size_t i = 0; i < mesh.tri_count; ++i) {
            const glm::vec3 v0 = model * glm::vec4(mesh.vertices[mesh.indices[i * 3 + 0]], 1.0f);
            const glm::vec3 v1 = model * glm::vec4(mesh.vertices[mesh.indices[i * 3 + 1]], 1.0f);
            const glm::vec3 v2 = model * glm::vec4(mesh.vertices[mesh.indices[i * 3 + 2]], 1.0f);
            triangles.emplace_back(v0, v1, v2, mesh.material);
            const glm::vec3 n0 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[mesh.indices[i * 3 + 0]], 0.0f)));
            const glm::vec3 n1 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[mesh.indices[i * 3 + 1]], 0.0f)));
            const glm::vec3 n2 = glm::normalize(glm::vec3(model * glm::vec4(mesh.normals[mesh.indices[i * 3 + 2]], 0.0f)));
            normals.emplace_back(n0, n1, n2);
        }
    }

    /* Build a BVH over all the triangles in the scene */
    bvh.build(triangles.data(), normals.data(), triangles.size());
}

}  // namespace wyre
