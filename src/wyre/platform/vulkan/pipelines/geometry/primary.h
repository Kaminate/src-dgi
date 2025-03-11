/**
 * @file pipelines/primary.h
 * @brief Vulkan primary pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

#include "vulkan/hardware/buffer.h" /* buf::* */

namespace wyre {

class Window;
class Logger;
class Device;

struct DescriptorSet;

/**
 * @brief Vulkan primary pass pipeline.
 */
class PrimaryPipeline {
    friend class GeometryStage;

    /* Shaders */
    vk::ShaderModule primary_shader = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    PrimaryPipeline() = delete;
    explicit PrimaryPipeline(Logger& logger, const Device& device, const DescriptorSet& bvh);
    ~PrimaryPipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Window& window, const Device& device, const DescriptorSet& bvh);
};

}  // namespace wyre
