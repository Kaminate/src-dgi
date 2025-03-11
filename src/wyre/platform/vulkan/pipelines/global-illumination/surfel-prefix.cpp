/**
 * @file pipelines/surfel-prefix.cpp
 * @brief Vulkan Surfel prefix sum pass pipeline.
 */
#include "surfel-prefix.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/log.h"

#include "cascade.h"

namespace wyre {

/* Shaders */
const char* PREFIX_SUM_SHADER = "assets/shaders/prefix-sum/prefix_sum.slang.spv";
const char* PREFIX_SEGMENTS_SHADER = "assets/shaders/prefix-sum/prefix_segments.slang.spv";
const char* PREFIX_MERGE_SHADER = "assets/shaders/prefix-sum/prefix_merge.slang.spv";

#define THREAD_GROUP_SIZE 512
#define SEGMENT_SIZE (THREAD_GROUP_SIZE * 2)

uint32_t round_nearest_pow2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

/* Re-used code for creating a pipeline */
inline bool prefix_pipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade, const DescriptorSet& desc_set, vk::ShaderModule shader_mod, vk::PipelineLayout& out_layout, vk::Pipeline& out_pipeline) {
    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(shader_mod, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(cascade.desc_set.layout);
    builder.add_descriptor_set(desc_set.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel prefix sum pipeline layout.");
            return false;
        }
        out_layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, out_layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel prefix sum graphics pipeline.");
            return false;
        }
        out_pipeline = result.value;
    }

    return true;
}

SurfelPrefixPipeline::SurfelPrefixPipeline(Logger& logger, const Device& device, const SurfelCascadeResources& cascade) {
    /* Load the surfel draw compute shader module */
    shader_sum = shader::from_file(device.device, PREFIX_SUM_SHADER).expect("failed to load surfel prefix sum shader.");
    shader_segments = shader::from_file(device.device, PREFIX_SEGMENTS_SHADER).expect("failed to load surfel prefix sum shader.");
    shader_merge = shader::from_file(device.device, PREFIX_MERGE_SHADER).expect("failed to load surfel prefix sum shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded surfel prefix sum compute shader modules.");

    DescriptorBuilder desc_builder{};
    desc_builder.add_binding(0, vk::DescriptorType::eStorageBuffer);
    segments_set = desc_builder.build(device, vk::ShaderStageFlagBits::eCompute);

    if (prefix_pipeline(logger, device, cascade, segments_set, shader_sum, layout_sum, pipeline_sum) == false) return;
    if (prefix_pipeline(logger, device, cascade, segments_set, shader_segments, layout_segments, pipeline_segments) == false) return;
    if (prefix_pipeline(logger, device, cascade, segments_set, shader_merge, layout_merge, pipeline_merge) == false) return;

    const buf::AllocParams alloc_ci{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

    /* Allocate the Segments buffer */
    const uint32_t segments_size = sizeof(uint32_t) * THREAD_GROUP_SIZE;
    if (!buf::alloc(device, segments_buffer, {segments_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create surfel prefix sum segments buffer.");
        return;
    }
    segments_set.attach_storage_buffer(device, 0, segments_buffer.buffer, segments_buffer.size);

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized surfel prefix sum pipeline.");
}

/**
 * @brief Push surfel accelerate pipeline commands into the graphics command buffer.
 */
void SurfelPrefixPipeline::enqueue(const Device& device, const SurfelCascadeResources& cascade) {
    /* Fetch the command buffer */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_store_desc;

    const uint32_t pc = (cascade.cascade_index & 0xFFFF) | (device.fid << 16u);
    const uint32_t cells = cascade.surfel_grid.size / sizeof(uint32_t);
    const uint32_t sums = round_nearest_pow2(cells);
    
    /* Clear the Segments buffer */
    cmd.fillBuffer(segments_buffer.buffer, 0u, segments_buffer.size, 0x00);
    
    /* Buffer memory barrier on the Surfel Hash Grid buffer */
    buf::barrier(cmd, cascade.surfel_grid, 0, cascade.surfel_grid.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_sum);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout_sum, 0u, {cascade.desc_set.set, segments_set.set}, {});
    cmd.pushConstants(layout_sum, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch(cells / THREAD_GROUP_SIZE, 1, 1);
    
    /* Buffer memory barrier on the Surfel Hash Grid buffer */
    buf::barrier(cmd, cascade.surfel_grid, 0, cascade.surfel_grid.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    /* Buffer memory barrier on the Segments buffer */
    buf::barrier(cmd, segments_buffer, 0, segments_buffer.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderWrite);

    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_segments);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout_segments, 0u, {cascade.desc_set.set, segments_set.set}, {});
    cmd.pushConstants(layout_segments, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch(1, 1, 1);

    /* Buffer memory barrier on the Segments buffer */
    buf::barrier(cmd, segments_buffer, 0, segments_buffer.size, 
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    
    /* Setup for executing the pipeline */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_merge);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout_merge, 0u, {cascade.desc_set.set, segments_set.set}, {});
    cmd.pushConstants(layout_merge, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &pc);

    /* Dispatch the kernel */
    cmd.dispatch(cells / THREAD_GROUP_SIZE, 1, 1);
}

void SurfelPrefixPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(shader_sum);
    device.device.destroyShaderModule(shader_segments);
    device.device.destroyShaderModule(shader_merge);

    segments_buffer.free(device);
    segments_set.free(device);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout_sum);
    device.device.destroyPipelineLayout(layout_segments);
    device.device.destroyPipelineLayout(layout_merge);
    device.device.destroyPipeline(pipeline_sum);
    device.device.destroyPipeline(pipeline_segments);
    device.device.destroyPipeline(pipeline_merge);
}

}  // namespace wyre
