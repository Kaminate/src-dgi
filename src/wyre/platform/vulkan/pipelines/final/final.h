/**
 * @file pipelines/final.h
 * @brief Vulkan final pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;

/**
 * @brief Vulkan final pass pipeline.
 */
class FinalPipeline {
    friend class FinalStage;

    /* Shaders */
    vk::ShaderModule final_shader = nullptr;

    vk::PipelineLayout layout = nullptr;
    vk::Pipeline pipeline = nullptr;

    FinalPipeline() = delete;
    explicit FinalPipeline(Logger& logger, const Window& window, const Device& device);
    ~FinalPipeline() = default;

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
