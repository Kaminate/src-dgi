/**
 * @file pipelines/surfel-draw.h
 * @brief Vulkan surfel debug draw pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan surfel debug draw pass pipeline.
 */
class SurfelDrawPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule draw_shader = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelDrawPipeline() = delete;
    explicit SurfelDrawPipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelDrawPipeline() = default;

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
