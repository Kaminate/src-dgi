/**
 * @file pipelines/surfel-gather.h
 * @brief Vulkan Surfel gathering pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
class Window;

struct SurfelCascadeResources;
struct DescriptorSet;

/**
 * @brief Vulkan Surfel gathering pass pipeline.
 */
class SurfelGatherPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelGatherPipeline() = delete;
    explicit SurfelGatherPipeline(Logger& logger, const Device& device, const DescriptorSet& bvh, const SurfelCascadeResources& cascade);
    ~SurfelGatherPipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Window& window, const Device& device, const DescriptorSet& bvh, const SurfelCascadeResources& cascade);
};

}  // namespace wyre
