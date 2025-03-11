/**
 * @file pipelines/surfel-spawn.cpp
 * @brief Vulkan Surfel spawn pass pipeline.
 */
#include "surfel-spawn.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"
#include "wyre/core/system/window.h"

#include "cascade.h"

namespace wyre {

/* SAS populate shader */
const char* SURFEL_SPAWN_SHADER = "assets/shaders/surfels/spawn.slang.spv";

SurfelSpawnPipeline::SurfelSpawnPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_SPAWN_SHADER).expect("failed to load surfel spawn shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel spawn compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(device.get_frame().attach_render_desc.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel spawn pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel spawn graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel spawn pipeline.");
}

/**
 * @brief Push surfel spawn pipeline commands into the graphics command buffer.
 */
void SurfelSpawnPipeline::enqueue(const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
    /* Fetch the command buffer */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const DescriptorSet& desc_set = device.get_frame().attach_render_desc;
    const img::RenderAttachment& albedo = device.get_frame().albedo;
    const img::RenderAttachment& normal_depth = device.get_frame().normal_depth;

    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);

    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cascade.desc_set.set, desc_set.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch((uint32_t)ceil((float)window.width / 16.0f), (uint32_t)ceil((float)window.height / 16.0f), 1);
}

void SurfelSpawnPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
