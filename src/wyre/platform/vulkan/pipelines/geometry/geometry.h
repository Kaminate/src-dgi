/**
 * @file pipelines/geometry.h
 * @brief Vulkan geometry pipeline.
 */
#pragma once

#include "vulkan/api.h"

#include "vulkan/hardware/buffer.h" /* buf::* */

namespace vk {
class Device;
class CommandBuffer;
}

namespace wyre {

class Window;
class Logger;
class Device;

/**
 * @brief Vulkan geometry pipeline.
 */
class GeometryPipeline {
    friend class GeometryStage;

    /* Shaders */
    vk::ShaderModule uber_shader = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    buf::Buffer vertex_buffer {};
    buf::Buffer index_buffer {};

    GeometryPipeline() = delete;
    explicit GeometryPipeline(Logger& logger, const Window& window, const Device& device);
    ~GeometryPipeline() = default;

    /**
     * @brief Destroy any pipeline resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Execute the pipeline.
     */
    void enqueue(const Window& window, const Device& device);
};

}  // namespace wyre
