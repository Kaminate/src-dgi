/**
 * @file pipelines/surfel-accel.h
 * @brief Vulkan Surfel acceleration pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel acceleration pass pipeline.
 */
class SurfelAccelerationPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelAccelerationPipeline() = delete;
    explicit SurfelAccelerationPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelAccelerationPipeline() = default;

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
