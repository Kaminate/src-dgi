/**
 * @file pipelines/surfel-spawn.h
 * @brief Vulkan Surfel spawn pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Logger;
class Device;
class Window;
struct SurfelCascadeResources;

/**
 * @brief Vulkan Surfel spawn pass pipeline.
 */
class SurfelSpawnPipeline {
    friend class GIStage;

    /* Shaders */
    vk::ShaderModule shader_mod = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    SurfelSpawnPipeline() = delete;
    explicit SurfelSpawnPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade);
    ~SurfelSpawnPipeline() = default;

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
