#pragma once

#include <cstdint> /* uint32_t */

#include "vulkan/hardware/buffer.h"
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/descriptor.h"

namespace wyre {

class Device;

/* TODO: Should probably have this as a parameter. */
constexpr uint32_t CASCADE_COUNT = 6u;

/** @brief Settings/parameters for the Surfel Cascades. */
struct SurfelCascadeParameters {
    /* `[c0]` Capacity of the hash grid structure. */
    uint32_t c0_grid_capacity = 0u;
    /* `[c0]` Scale of grid cells in the hash grid structure. */
    float c0_grid_scale = 0.0f;
    /* `[cN]` Surfel acceleration structure grid cell Surfel capacity. */
    uint32_t cell_capacity = 0u;
    /* NOTE: Radiance data is stored in a texture, e.g. 16 intervals = 4x4 square. */
    /* `[c0]` Square root of the number of intervals per Surfel probe. */
    uint32_t c0_memory_width = 0u;
    /* `[c0]` Maximum number of active Surfel probes. */
    uint32_t c0_probe_capacity = 0u;
    /* `[c0]` Surfel probe radius in screen-space. */
    float c0_probe_radius = 0.0f;
    /* `[cN]` Maximum projected solid angle of intervals. *(used to derive interval length)* */
    float max_solid_angle = 0.0f;

    SurfelCascadeParameters();

    /* `[cN]` Get the square root of the number of intervals per Surfel probe. */
    uint32_t get_memory_width(const uint32_t cascade_index) const;
    
    /* `[cN]` Get the maximum number of active Surfel probes. */
    uint32_t get_probe_capacity(const uint32_t cascade_index) const;
    
    /* `[cN]` Get the capacity of the hash grid structure. */
    uint32_t get_grid_capacity(const uint32_t cascade_index) const;
};

/** @brief GPU Surfel Cascade resources. */
struct SurfelCascadeResources {
    /* Buffers */
    buf::Buffer surfel_param{};    /* (0)  [R] Surfel Cascade parameters. */
    buf::Buffer surfel_stack{};    /* (1) [RW] Tracks live Surfels. [0] = Stack pointer. */
    buf::Buffer surfel_grid{};     /* (2) [RW] Surfel Hash Grid entry indices. */
    buf::Buffer surfel_list{};     /* (3) [RW] Surfel Hash Grid entries list. */
    buf::Buffer surfel_posr{};     /* (4) [RW] `xyz = position, w = radius` */
    buf::Buffer surfel_norw{};     /* (5) [RW] `xyz = normal, w = unused` */
    img::Texture2D surfel_rad{};   /* (6) [RW] Radiance cache */
    img::Texture2D surfel_merge{}; /* (7) [RW] Merged radiance cache */
    vk::Sampler surfel_rad_sampler{};

    DescriptorSet desc_set{};
    uint32_t surfel_count = 0u;
    uint32_t cascade_index = 0u;

    SurfelCascadeResources() = default;
    SurfelCascadeResources(const Device& device);

    /** @brief Allocate the Surfel Cascade resources. */
    bool alloc(const Device& device, const SurfelCascadeParameters& params, const uint32_t cascade_index);

    /** @brief Free the Surfel Cascade buffers only, leaving the descriptor set intact. */
    void free_buffers(const Device& device);

    /** @brief Free the Surfel Cascade resources. */
    void free(const Device& device);

    /** @brief Update the live Surfel count. */
    bool update_surfel_count(const Device& device, const SurfelCascadeParameters& params);
};

}  // namespace wyre
