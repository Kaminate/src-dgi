/**
 * @file stages/final.cpp
 * @brief Vulkan final rendering stage. (outputs to the swapchain)
 */
#include "final.h"

#include "vulkan/pipelines/final/final.h" /* FinalPipeline */
#include "vulkan/pipelines/final/overlay.h" /* OverlayPipeline */

namespace wyre {

FinalStage::FinalStage(Logger& logger, const Window& window, const Device& device) 
    : final_pipeline(*new FinalPipeline(logger, window, device)),
    overlay_pipeline(*new OverlayPipeline(logger, window, device)) {}

/**
 * @brief Push final stage commands into the graphics command buffer.
 */
void FinalStage::enqueue(const Window& window, const Device& device) {
    final_pipeline.enqueue(window, device);

#if 1
    /* TODO: Only include the overlay pass for development. */
    overlay_pipeline.enqueue(window, device);
#endif
}

void FinalStage::destroy(const Device& device) {
    final_pipeline.destroy(device);
    delete &final_pipeline;
    overlay_pipeline.destroy(device);
    delete &overlay_pipeline;
}

}  // namespace wyre
