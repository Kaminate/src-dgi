/**
 * @file stages/global-illumination.h
 * @brief Vulkan global illumination rendering stage.
 */
#pragma once

#include "vulkan/api.h"

#include "vulkan/pipelines/global-illumination/cascade.h" /* SurfelCascadeResources */

namespace wyre {

class Window;
class Logger;
class Device;

class SurfelCountPipeline;
class SurfelPrefixPipeline;
class SurfelAccelerationPipeline;
class SurfelSpawnPipeline;
class SurfelGatherPipeline;
class SurfelMergePipeline;
class SurfelCompositePipeline;
class SurfelRecyclePipeline;

/* Debug pipelines */
class SurfelDrawPipeline;
class SurfelHeatmapPipeline;
class GroundTruthPipeline;

/**
 * @brief Vulkan global illumination rendering stage.
 */
class GIStage {
    friend class Renderer;

    /* Surfel Cascade resources */
    SurfelCascadeParameters cascade_params{};
    SurfelCascadeResources cascade_dummy{};
    SurfelCascadeResources cascades[CASCADE_COUNT]{};

    SurfelCountPipeline& surfel_count_pipeline;
    SurfelPrefixPipeline& surfel_prefix_pipeline;
    SurfelAccelerationPipeline& surfel_accel_pipeline;
    SurfelSpawnPipeline& surfel_spawn_pipeline;
    SurfelGatherPipeline& surfel_gather_pipeline;
    SurfelMergePipeline& surfel_merge_pipeline;
    SurfelCompositePipeline& surfel_composite_pipeline;
    SurfelRecyclePipeline& surfel_recycle_pipeline;

    /* Debug pipelines */
    SurfelDrawPipeline& surfel_debug_pipeline;
    bool direct_draw = false;
    SurfelHeatmapPipeline& surfel_heatmap_pipeline;
    bool heatmap = false;
    uint32_t debug_cascade_index = 0u;
    GroundTruthPipeline& ground_truth_pipeline;
    bool ground_truth = false;

    GIStage() = delete;
    explicit GIStage(Logger& logger, const Window& window, const Device& device, const DescriptorSet& bvh);
    ~GIStage() = default;

    void init_resources(Logger& logger, const Device& device);
    void free_resources(const Device& device);

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Window& window, const Device& device, const DescriptorSet& bvh);

    /**
     * @brief Update the GI parameters.
     */
    void update_params(Logger& logger, const Device& device);
};

}  // namespace wyre
