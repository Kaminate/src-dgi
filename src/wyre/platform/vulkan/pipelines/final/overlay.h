/**
 * @file pipelines/overlay.h
 * @brief Vulkan ImGui pass pipeline.
 */
#pragma once

#include "vulkan/api.h"

namespace wyre {

class Window;
class Logger;
class Device;

/**
 * @brief Vulkan ImGui pass pipeline.
 */
class OverlayPipeline {
    friend class FinalStage;

    vk::DescriptorPool desc_pool {};

    OverlayPipeline() = delete;
    explicit OverlayPipeline(Logger& logger, const Window& window, const Device& device);
    ~OverlayPipeline() = default;

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
