/**
 * @file pipelines/ground-truth.h
 * @brief Vulkan ground truth pass pipeline.
 */
#pragma once

#include "vulkan/api.h"
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/descriptor.h"

namespace wyre {

class Logger;
class Device;
class Window;

struct DescriptorSet;

/**
 * @brief Vulkan ground truth pass pipeline.
 */
class GroundTruthPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    img::Texture2D radiance_cache{};
    DescriptorSet cache_set{};

    GroundTruthPipeline() = delete;
    explicit GroundTruthPipeline(Logger& logger, const Device& device, const Window& window, const DescriptorSet& bvh);
    ~GroundTruthPipeline() = default;

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
