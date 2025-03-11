/**
 * @file pipelines/ground-truth.cpp
 * @brief Vulkan ground truth pass pipeline.
 */
#include "ground-truth.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"
#include "wyre/core/system/window.h"

namespace wyre {

/* SAS populate shader */
const char* SURFEL_GT_SHADER = "assets/shaders/ray-tracing/ground_truth.slang.spv";

struct context_t {
    float alpha;
    uint32_t frame_index;
};

GroundTruthPipeline::GroundTruthPipeline(Logger& logger, const Device& device, const Window& window, const DescriptorSet& bvh) {
    /* Load the compute shader module */
    shader_mod = shader::from_file(device.device, SURFEL_GT_SHADER).expect("failed to load ground truth shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded ground truth compute shader module.");

    DescriptorBuilder desc_builder{};
    desc_builder.add_binding(0, vk::DescriptorType::eStorageImage);
    cache_set = desc_builder.build(device, vk::ShaderStageFlagBits::eCompute);
    
    img::Texture2D::make(device, radiance_cache, 
        {window.width, window.height}, 
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageAspectFlagBits::eColor, 
        vk::ImageLayout::eGeneral, 
        vk::ImageUsageFlagBits::eStorage
    );
    cache_set.attach_storage_image(device, 0, radiance_cache.view, device.nearest_sampler, vk::ImageLayout::eGeneral);

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cache_set.layout);
    builder.add_descriptor_set(device.get_frame().attach_store_desc.layout);
    builder.add_descriptor_set(bvh.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(context_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create ground truth pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create ground truth graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized ground truth pipeline.");
}

/**
 * @brief Push surfel gather pipeline commands into the graphics command buffer.
 */
void GroundTruthPipeline::enqueue(const Window& window, const Device& device, const DescriptorSet& bvh) {
    /* Fetch the command buffer */
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
    img::barrier(cmd, radiance_cache.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eUndefined,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);

    const context_t pc {1.0f / 320.0f, device.fid};
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {cache_set.set, desc_set.set, bvh.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(context_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch((uint32_t)ceil((float)window.width / 8.0f), (uint32_t)ceil((float)window.height / 8.0f), 1);
}

void GroundTruthPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_mod);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);

    cache_set.free(device);
    radiance_cache.free(device);
}

}  // namespace wyre
