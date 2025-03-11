/**
 * @file pipelines/surfel-draw.cpp
 * @brief Vulkan surfel debug draw pass pipeline.
 */
#include "surfel-draw.h"

#include <glm/ext/matrix_transform.hpp>

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"

#include "cascade.h"

namespace wyre {

/* Surfel draw shader */
const char* SURFEL_DRAW_SHADER = "assets/shaders/surfels/direct_draw.slang.spv";

SurfelDrawPipeline::SurfelDrawPipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    draw_shader = shader::from_file(device.device, SURFEL_DRAW_SHADER).expect("failed to load surfel draw shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel draw compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(draw_shader, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(device.get_frame().attach_store_desc.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel draw pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel draw graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel draw pipeline.");
}

/**
 * @brief Push surfel draw pipeline commands into the graphics command buffer.
 */
void SurfelDrawPipeline::enqueue(const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const img::RenderAttachment& albedo = device.get_frame().albedo;
    const img::RenderAttachment& normal_depth = device.get_frame().normal_depth;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_store_desc;

    /* Place a memory barrier on the albedo output */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    img::barrier(cmd, normal_depth.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);

    /* Buffer memory barrier on the surfel acceleration structure buffer */
    buf::barrier(cmd, cascade.surfel_grid, 0, cascade.surfel_grid.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    buf::barrier(cmd, cascade.surfel_stack, 0, cascade.surfel_stack.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);

    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);

    /* Setup for rendering */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cascade.desc_set.set, desc_set.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Draw */
    cmd.dispatch((uint32_t)ceil((float)window.width / 16.0f), (uint32_t)ceil((float)window.height / 8.0f), 1);
}

void SurfelDrawPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(draw_shader);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
