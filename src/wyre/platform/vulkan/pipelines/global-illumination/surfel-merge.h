/**
 * @file pipelines/surfel-merge.h
 * @brief Vulkan Surfel merge pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel merge pass pipeline.
 */
class SurfelMergePipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelMergePipeline() = delete;
    explicit SurfelMergePipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelMergePipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Device& device, const SurfelCascadeResources& src_cascade, const SurfelCascadeResources& dst_cascade);
};

}  // namespace wyre
