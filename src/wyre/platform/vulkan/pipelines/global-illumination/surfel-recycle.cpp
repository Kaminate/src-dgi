/**
 * @file pipelines/surfel-recycle.cpp
 * @brief Vulkan Surfel recycling pass pipeline.
 */
#include "surfel-recycle.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"

#include "cascade.h"

namespace wyre {

/* SAS populate shader */
const char* SURFEL_RECYCLE_SHADER = "assets/shaders/surfels/recycle.slang.spv";

SurfelRecyclePipeline::SurfelRecyclePipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_RECYCLE_SHADER).expect("failed to load surfel recycle shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel recycle compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(device.get_frame().attach_store_desc.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel recycle pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel recycle graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel recycle pipeline.");
}

/**
 * @brief Push surfel accelerate pipeline commands into the graphics command buffer.
 */
void SurfelRecyclePipeline::enqueue(const Device& device, const SurfelCascadeResources& cascade) {
    /* Fetch the command buffer */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_store_desc;

    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cascade.desc_set.set, desc_set.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch((cascade.surfel_stack.size / sizeof(uint32_t) + 255) / 256, 1, 1);
}

void SurfelRecyclePipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
