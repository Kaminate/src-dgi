/**
 * @file stages/geometry.cpp
 * @brief Vulkan geometry rendering stage. (fills the GBuffers)
 */
#include "geometry.h"

#include "vulkan/pipelines/geometry/primary.h" /* PrimaryPipeline */
#include "vulkan/hardware/debug.h" /* begin_label() */

namespace wyre {

GeometryStage::GeometryStage(Logger& logger, const Device& device, const DescriptorSet& bvh) : primary_pipeline(*new PrimaryPipeline(logger, device, bvh)) {}

/**
 * @brief Push geometry stage commands into the graphics command buffer.
 */
void GeometryStage::enqueue(const Window& window, const Device& device, const DescriptorSet& bvh) {
    debug::begin_label(device, "Geometry Pass", {0.659f, 0.988f, 0.192f});
    primary_pipeline.enqueue(window, device, bvh);
    debug::end_label(device);
}

void GeometryStage::destroy(const Device& device) {
    primary_pipeline.destroy(device);
    delete &primary_pipeline;
}

}  // namespace wyre
