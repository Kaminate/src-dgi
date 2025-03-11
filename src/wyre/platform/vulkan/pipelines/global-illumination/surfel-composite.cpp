/**
 * @file pipelines/surfel-composite.cpp
 * @brief Vulkan surfel composite pass pipeline.
 */
#include "surfel-composite.h"

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
const char* SURFEL_COMPOSITE_SHADER = "assets/shaders/surfels/composite.slang.spv";

SurfelCompositePipeline::SurfelCompositePipeline(Logger& logger, const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    draw_shader = shader::from_file(device.device, SURFEL_COMPOSITE_SHADER).expect("failed to load surfel composite shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel composite compute shader module.");

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
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel composite pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel composite graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel composite pipeline.");
}

/**
 * @brief Push surfel composite pipeline commands into the graphics command buffer.
 */
void SurfelCompositePipeline::enqueue(const Window& window, const Device& device, const SurfelCascadeResources& cascade) {
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
    img::barrier(cmd, cascade.surfel_rad.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    
    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);

    /* Setup for rendering */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cascade.desc_set.set, desc_set.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Draw */
    cmd.dispatch((uint32_t)ceil((float)window.width / 8.0f), (uint32_t)ceil((float)window.height / 8.0f), 1);
}

void SurfelCompositePipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(draw_shader);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
