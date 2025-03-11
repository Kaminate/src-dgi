/**
 * @file stages/geometry.h
 * @brief Vulkan geometry rendering stage. (fills the GBuffers)
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;
struct DescriptorSet;

class PrimaryPipeline;

/**
 * @brief Vulkan geometry rendering stage. (fills the GBuffers)
 */
class GeometryStage {
    friend class Renderer;

    /* Pipelines */
    PrimaryPipeline& primary_pipeline;

    GeometryStage() = delete;
    explicit GeometryStage(Logger& logger, const Device& device, const DescriptorSet& bvh);
    ~GeometryStage() = default;

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
