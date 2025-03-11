/**
 * @file pipelines/surfel-composite.h
 * @brief Vulkan surfel composite pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan surfel composite pass pipeline.
 */
class SurfelCompositePipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule draw_shader = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelCompositePipeline() = delete;
    explicit SurfelCompositePipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelCompositePipeline() = default;

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
