/**
 * @file pipelines/surfel-heatmap.cpp
 * @brief Vulkan surfel heatmap debug draw pass pipeline.
 */
#include "surfel-heatmap.h"

#include <glm/ext/matrix_transform.hpp>

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"

#include "cascade.h"

namespace wyre {

/* Surfel heatmap shader */
const char* SURFEL_HEATMAP_SHADER = "assets/shaders/surfels/heatmap.slang.spv";

SurfelHeatmapPipeline::SurfelHeatmapPipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_HEATMAP_SHADER).expect("failed to load surfel heatmap shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel heatmap compute shader module.");

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
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel heatmap pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel heatmap graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel heatmap pipeline.");
}

/**
 * @brief Push surfel draw pipeline commands into the graphics command buffer.
 */
void SurfelHeatmapPipeline::enqueue(const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
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
    cmd.dispatch((uint32_t)ceil((float)window.width / 16.0f), (uint32_t)ceil((float)window.height / 16.0f), 1);
}

void SurfelHeatmapPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
