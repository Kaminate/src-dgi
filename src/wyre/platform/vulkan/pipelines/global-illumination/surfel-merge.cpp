/**
 * @file pipelines/surfel-merge.cpp
 * @brief Vulkan Surfel merge pass pipeline.
 */
#include "surfel-merge.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"

#include "cascade.h"

namespace wyre {

/* SAS populate shader */
const char* SURFEL_MERGE_SHADER = "assets/shaders/surfels/merge.slang.spv";

SurfelMergePipeline::SurfelMergePipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_MERGE_SHADER).expect("failed to load surfel merge shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel merge compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(device.get_frame().attach_store_desc.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel merge pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel merge graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel merge pipeline.");
}

/**
 * @brief Push surfel merge pipeline commands into the graphics command buffer.
 */
void SurfelMergePipeline::enqueue(const Device& device, const SurfelCascadeResources& src_cascade, const SurfelCascadeResources& dst_cascade) {
    /* Fetch the command buffer */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_store_desc;

    /* Buffer memory barrier on the surfel norw buffer */
    // buf::barrier(cmd, src_cascade.surfel_stack, 0, src_cascade.surfel_stack.size, 
    //     /* Src */ buf::PStage::eComputeShader, buf::Access::eShaderRead,
    //     /* Dst */ buf::PStage::eComputeShader, buf::Access::eShaderWrite);
    // buf::barrier(cmd, dst_cascade.surfel_stack, 0, dst_cascade.surfel_stack.size, 
    //     /* Src */ buf::PStage::eComputeShader, buf::Access::eShaderRead,
    //     /* Dst */ buf::PStage::eComputeShader, buf::Access::eShaderWrite);
    // /* Place a memory barrier on the surfel radiance cache */
    // img::barrier(cmd, src_cascade.surfel_rad.image,
    //     /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
    //     /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    // img::barrier(cmd, dst_cascade.surfel_rad.image,
    //     /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
    //     /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    
    const uint32_t pc = (dst_cascade.cascade_index & 0xFFFF) | (device.fid << 16u);
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {dst_cascade.desc_set.set, src_cascade.desc_set.set, desc_set.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch((dst_cascade.surfel_rad.width + 15) / 16, (dst_cascade.surfel_rad.height + 15) / 16, 1);
}

void SurfelMergePipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
