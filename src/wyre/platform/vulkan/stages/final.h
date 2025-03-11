/**
 * @file stages/final.h
 * @brief Vulkan final rendering stage. (outputs to the swapchain)
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;

class FinalPipeline;
class OverlayPipeline;

/**
 * @brief Vulkan final rendering stage. (outputs to the swapchain)
 */
class FinalStage {
    friend class Renderer;

    /* Pipelines */
    FinalPipeline& final_pipeline;
    OverlayPipeline& overlay_pipeline;

    FinalStage() = delete;
    explicit FinalStage(Logger& logger, const Window& window, const Device& device);
    ~FinalStage() = default;

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
