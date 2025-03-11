#include "bvh.h"

namespace wyre::scene {

Bvh::Bvh(const Triangle* prims, const Normals* norms, const uint32_t prim_count) { build(prims, norms, size); }

/** @brief Refit a node to include a list of primitives. */
inline void refit_node(Bvh::Node& node, const Triangle* prims) {
    /* Reset the node bounds */
    node.min = glm::vec3(1e30f);
    node.max = glm::vec3(-1e30f);

    /* Refit to snuggly include all primitives */
    for (uint32_t i = 0; i < node.prim_count; ++i) {
        const Triangle& prim = prims[node.left_first + i];

        node.min = glm::min(node.min, prim.v0);
        node.min = glm::min(node.min, prim.v1);
        node.min = glm::min(node.min, prim.v2);
        node.max = glm::max(node.max, prim.v0);
        node.max = glm::max(node.max, prim.v1);
        node.max = glm::max(node.max, prim.v2);
    }
}

void Bvh::build(const Triangle* new_prims, const Normals* new_norms, const uint32_t _prim_count) {
    if (new_prims == nullptr || _prim_count == 0u || new_norms == nullptr) return;
    prim_count = _prim_count;

    /* Allocate space for primitives and copy primitives over */
    if (prims) delete[] prims;
    if (norms) delete[] norms;
    prims = new Triangle[prim_count]{};
    norms = new Normals[prim_count]{};
    memcpy(prims, new_prims, prim_count * sizeof(Triangle));
    memcpy(norms, new_norms, prim_count * sizeof(Normals));
    size = prim_count;

    /* Allocate space for BVH nodes */
    nodes = (Node*)_aligned_malloc(sizeof(Node) * size * 2, 64);

    /* Initialize the root node */
    Node& root = nodes[root_idx];
    root.left_first = 0, root.prim_count = size;
    nodes_used = 2; /* Skip the second node, for better child node cache alignment */
    refit_node(root, prims);

    /* Begin the recursive subdivide */
    subdivide(root);

    /* Allocate space for GPU optimized nodes */
    if (gpu_nodes) delete[] gpu_nodes;
    gpu_nodes = new GPUNode[nodes_used]{};

    /* Convert CPU nodes to GPU optimized format */
    uint32_t alt_node = 0, node_ptr = 0, stack[128], stack_ptr = 0;
    for (;;) { /* Credit: <https://github.com/jbikker/tinybvh> */
        const Node& node = nodes[node_ptr];
        const unsigned idx = alt_node++;
        if (node.is_leaf()) {
            gpu_nodes[idx].prim_count = node.prim_count;
            gpu_nodes[idx].prim_index = node.left_first;
            if (!stack_ptr) break;
            node_ptr = stack[--stack_ptr];
            unsigned newNodeParent = stack[--stack_ptr];
            gpu_nodes[newNodeParent].right = alt_node; /* <- right filled here */
            continue;
        }
        const Node& left = nodes[node.left_first];
        const Node& right = nodes[node.left_first + 1];
        gpu_nodes[idx].lmin = left.min, gpu_nodes[idx].rmin = right.min;
        gpu_nodes[idx].lmax = left.max, gpu_nodes[idx].rmax = right.max;
        gpu_nodes[idx].left = alt_node; /* right will be filled when popped! */
        stack[stack_ptr++] = idx;
        stack[stack_ptr++] = node.left_first + 1;
        node_ptr = node.left_first;
    }
}

void Bvh::subdivide(Node& node, const uint32_t depth) {
    if (node.prim_count <= 2u) return;

    /* Determine split based on SAH */
    int split_axis = -1;
    float split_t = 0.0f;
    const float split_cost = find_best_split(node, split_axis, split_t);

    /* Calculate parent node cost */
    const glm::vec3 e = node.max - node.min;
    const float parent_area = e.x * e.x + e.y * e.y + e.z * e.z;
    const float parent_cost = node.prim_count * parent_area;
    if (split_cost >= parent_cost) return; /* Split would not be worth it */

    /* Determine which primitives lie on which side */
    uint32_t i = node.left_first;
    uint32_t j = i + node.prim_count - 1u;
    while (i <= j) {
        if (prims[i].get_centroid()[split_axis] < split_t) {
            i++;
        } else {
            std::swap(prims[i], prims[j]);
            std::swap(norms[i], norms[j]);
            j--;
        }
    }

    const uint32_t left_count = i - node.left_first;
    if (left_count == 0u || left_count == node.prim_count) return;

    /* Initialize the child nodes */
    const uint32_t left_child_idx = nodes_used++;
    const uint32_t right_child_idx = nodes_used++;
    nodes[left_child_idx].left_first = node.left_first;
    nodes[left_child_idx].prim_count = left_count;
    nodes[right_child_idx].left_first = i;
    nodes[right_child_idx].prim_count = node.prim_count - left_count;
    node.left_first = left_child_idx;
    node.prim_count = 0u;

    /* Refit the child nodes */
    refit_node(nodes[left_child_idx], prims);
    refit_node(nodes[right_child_idx], prims);

    /* Continue subdiving recursively */
    subdivide(nodes[left_child_idx], depth + 1u);
    subdivide(nodes[right_child_idx], depth + 1u);
}

float Bvh::find_best_split(const Node& node, int& axis, float& t) const {
    float lowest_cost = 1e30f;

    struct Bin {
        AABB aabb {};
        uint32_t prim_count = 0u;
    };

    constexpr uint32_t BINS = 8u;
    for (uint32_t a = 0u; a < 3u; ++a) {
        /* Get the min and max of all primitives in the node */
        float bmin = 1e30f, bmax = -1e30f;
        for (uint32_t i = 0u; i < node.prim_count; ++i) {
            const glm::vec3& prim_center = prims[node.left_first + i].get_centroid();
            bmin = fminf(bmin, prim_center[a]);
            bmax = fmaxf(bmax, prim_center[a]);
        }
        if (bmin == bmax) continue;

        /* Populate the bins */
        Bin bins[BINS];
        float scale = (float)BINS / (bmax - bmin);
        for (uint32_t i = 0u; i < node.prim_count; ++i) {
            const Triangle& prim = prims[node.left_first + i];
            const int bin_idx = (int)fminf(BINS - 1u, (float)((prim.get_centroid()[a] - bmin) * scale));
            bins[bin_idx].prim_count++;
            const AABB prim_bounds = prim.get_aabb();
            bins[bin_idx].aabb.grow(prim_bounds.min);
            bins[bin_idx].aabb.grow(prim_bounds.max);
        }

        /* Gather data for the planes between the bins */
        float l_areas[BINS - 1u], r_areas[BINS - 1u];
        uint32_t l_counts[BINS - 1u], r_counts[BINS - 1u];
        AABB l_aabb, r_aabb;
        uint32_t l_sum = 0u, r_sum = 0u;
        for (uint32_t i = 0u; i < BINS - 1u; ++i) {
            /* Left-side */
            l_sum += bins[i].prim_count;
            l_counts[i] = l_sum;
            l_aabb.grow(bins[i].aabb);
            l_areas[i] = l_aabb.area();
            /* Right-side */
            r_sum += bins[BINS - 1u - i].prim_count;
            r_counts[BINS - 2u - i] = r_sum;
            r_aabb.grow(bins[BINS - 1u - i].aabb);
            r_areas[BINS - 2u - i] = r_aabb.area();
        }

        /* Calculate the SAH cost function for all planes */
        scale = (bmax - bmin) / BINS;
        for (uint32_t i = 0u; i < BINS - 1u; ++i) {
            const float plane_cost = l_counts[i] * l_areas[i] + r_counts[i] * r_areas[i];
            if (plane_cost < lowest_cost) {
                axis = a;
                t = bmin + scale * (i + 1u);
                lowest_cost = plane_cost;
            }
        }
    }

    return lowest_cost;
}

float Bvh::eval_sah(const Node& node, const int axis, const float t) const {
    AABB left_aabb{}, right_aabb{};
    uint32_t left_count = 0, right_count = 0;
    for (uint32_t i = 0; i < node.prim_count; ++i) {
        const Triangle& prim = prims[node.left_first + i];
        const AABB aabb = prim.get_aabb();
        if (prim.get_centroid()[axis] < t) {
            left_count++;
            left_aabb.grow(aabb.min);
            left_aabb.grow(aabb.max);
        } else {
            right_count++;
            right_aabb.grow(aabb.min);
            right_aabb.grow(aabb.max);
        }
    }
    const float cost = left_count * left_aabb.area() + right_count * right_aabb.area();
    return cost > 0.0f ? cost : 1e30f;
}

}  // namespace wyre
