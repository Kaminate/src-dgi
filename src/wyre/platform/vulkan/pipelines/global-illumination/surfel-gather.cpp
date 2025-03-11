/**
 * @file pipelines/surfel-gather.cpp
 * @brief Vulkan Surfel gathering pass pipeline.
 */
#include "surfel-gather.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/hardware/descriptor.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"
#include "wyre/core/system/window.h"

#include "cascade.h"

namespace wyre {

/* SAS populate shader */
const char* SURFEL_GATHER_SHADER = "assets/shaders/surfels/gather.slang.spv";

SurfelGatherPipeline::SurfelGatherPipeline(Logger& logger, const Device& device, const DescriptorSet& bvh, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_GATHER_SHADER).expect("failed to load surfel gather shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel gather compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(bvh.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel gather pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel gather graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel gather pipeline.");
}

/**
 * @brief Push surfel gather pipeline commands into the graphics command buffer.
 */
void SurfelGatherPipeline::enqueue(const Window& window, const Device& device, const DescriptorSet& bvh, const SurfelCascadeResources& cascade) {
    /* Fetch the command buffer */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;

    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cascade.desc_set.set, bvh.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    // const uint32_t trace_width = (cascade.surfel_rad.width + 3) / 4;
    // const uint32_t trace_height = (cascade.surfel_rad.height + 3) / 4;
    const uint32_t trace_width = cascade.surfel_rad.width;
    const uint32_t trace_height = cascade.surfel_rad.height;
    cmd.dispatch((trace_width + 15) / 16, (trace_height + 15) / 16, 1);
}

void SurfelGatherPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
