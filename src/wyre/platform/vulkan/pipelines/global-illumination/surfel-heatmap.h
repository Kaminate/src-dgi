/**
 * @file pipelines/surfel-heatmap.h
 * @brief Vulkan surfel heatmap debug draw pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan surfel heatmap debug draw pass pipeline.
 */
class SurfelHeatmapPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelHeatmapPipeline() = delete;
    explicit SurfelHeatmapPipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelHeatmapPipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Window& window, const Device& device, const SurfelCascadeResources& cascade);
};

}  // namespace wyre
