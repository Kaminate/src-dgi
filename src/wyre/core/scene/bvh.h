#pragma once

#include <glm/glm.hpp>

#include "triangle.h"

namespace wyre::scene {

using Index = uint32_t;

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

/**
 * @brief Bounding Volume Hierarchy structure.
 */
struct Bvh {
    /* BVH Node structure. */
    struct Node {
        glm::vec3 min;
        uint32_t left_first;
        glm::vec3 max;
        uint32_t prim_count;
        /** @returns True if this node a leaf node. */
        bool is_leaf() const { return prim_count; }
    };

    /* BVH Node optimized for GPU ray tracing. */
    struct GPUNode {
        glm::vec3 lmin; uint32_t left;
        glm::vec3 lmax; uint32_t right;
        glm::vec3 rmin; uint32_t prim_index;
        glm::vec3 rmax; uint32_t prim_count;
    };

    Node* nodes = nullptr;
    /* Skip the second node, for better child node cache alignment. */
    uint32_t root_idx = 0, nodes_used = 2;
    uint32_t size = 2;

    /* Primitives. */
    Triangle* prims = nullptr;
    Normals* norms = nullptr;
    uint32_t prim_count = 0;

    /* Nodes parsed into a GPU optimized format. */
    GPUNode* gpu_nodes = nullptr;

    Bvh() = default;
    Bvh(const Triangle* prims, const Normals* norms, const uint32_t prim_count);

    /** @brief Evaluate the Surface Area Heuristic of a specific node split. */
    float eval_sah(const Node& node, const int axis, const float t) const;

    /** @brief Find the "optimal" axis & time along that axis to split a node. */
    float find_best_split(const Node& node, int& axis, float& t) const;

    /** @brief Sub-divide a given BVH node. */
    void subdivide(Node& node, const uint32_t depth = 0);

    /** @brief Build the BVH based on a collection of primitives. */
    void build(const Triangle* prims, const Normals* norms, const uint32_t prim_count);
};

}  // namespace wyre
