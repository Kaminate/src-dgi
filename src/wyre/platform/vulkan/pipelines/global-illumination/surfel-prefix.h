/**
 * @file pipelines/surfel-prefix.h
 * @brief Vulkan Surfel prefix sum pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

#include "vulkan/hardware/buffer.h"
#include "vulkan/hardware/descriptor.h"

namespace wyre {

class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel prefix sum pass pipeline.
 */
class SurfelPrefixPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_sum = nullptr;
    vk::ShaderModule shader_segments = nullptr;
    vk::ShaderModule shader_merge = nullptr;

    vk::PipelineLayout layout_sum = nullptr;
    vk::PipelineLayout layout_segments = nullptr;
    vk::PipelineLayout layout_merge = nullptr;
    vk::Pipeline pipeline_sum = nullptr;
    vk::Pipeline pipeline_segments = nullptr;
    vk::Pipeline pipeline_merge = nullptr;

    buf::Buffer segments_buffer{};
    DescriptorSet segments_set{};

    SurfelPrefixPipeline() = delete;
    explicit SurfelPrefixPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelPrefixPipeline() = default;

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
