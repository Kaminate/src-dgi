/**
 * @file pipelines/surfel-count.h
 * @brief Vulkan Surfel counting pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel counting pass pipeline.
 */
class SurfelCountPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelCountPipeline() = delete;
    explicit SurfelCountPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelCountPipeline() = default;

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
