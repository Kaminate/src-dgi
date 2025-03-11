/**
 * @file pipelines/surfel-recycle.h
 * @brief Vulkan Surfel recycling pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel recycling pass pipeline.
 */
class SurfelRecyclePipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelRecyclePipeline() = delete;
    explicit SurfelRecyclePipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelRecyclePipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Device& device, const SurfelCascadeResources& cascade);
};

}  // namespace wyre
