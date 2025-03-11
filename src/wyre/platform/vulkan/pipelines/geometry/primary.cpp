/**
 * @file pipelines/primary.cpp
 * @brief Vulkan primary pass pipeline.
 */
#include "primary.h"

#include <glm/ext/matrix_transform.hpp>

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/compute-builder.h"
#include "vulkan/hardware/descriptor.h"
#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"
#include "wyre/core/scene/bvh.h"

namespace wyre {

using namespace scene;

/* Primary shader */
const char* PRIMARY_SHADER = "assets/shaders/ray-tracing/primary.slang.spv";

struct PushConstants {
    glm::mat4 inv_vp;
    glm::vec3 origin;
};

PrimaryPipeline::PrimaryPipeline(Logger& logger, const Device& device, const DescriptorSet& bvh) {
    /* Load the primary compute shader module */
    primary_shader = shader::from_file(device.device, PRIMARY_SHADER).expect("failed to load primary shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded primary compute shader module.");

    ComputeBuilder builder{};
    /* Shader stages */
    builder.set_shader_entry(primary_shader, "main");
    /* Descriptor sets */
    builder.add_descriptor_set(device.get_frame().attach_store_desc.layout);
    builder.add_descriptor_set(bvh.layout);
    /* Add the push constants */
    builder.add_push_constants(sizeof(uint32_t));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create primary pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create primary graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized primary pipeline.");
}

/**
 * @brief Push primary pipeline commands into the graphics command buffer.
 */
void PrimaryPipeline::enqueue(const Window& window, const Device& device, const DescriptorSet& bvh) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const img::RenderAttachment& albedo = device.get_frame().albedo;
    const img::RenderAttachment& normal_depth = device.get_frame().normal_depth;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_store_desc;

    /* Transition albedo & normal attachment into general format for writing */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eBottomOfPipe, img::Access::eNone, img::Layout::eUndefined,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral);
    img::barrier(cmd, normal_depth.image,
        /* Src */ img::PStage::eBottomOfPipe, img::Access::eNone, img::Layout::eUndefined,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral);

    /* TEMP: Rotating cube over time... */
    static float time = 0.0f;
    time += 0.016f;

    /* Setup for rendering */
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0u, {desc_set.set, bvh.set}, {});
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0u, sizeof(uint32_t), &device.fid);

    /* Draw */
    cmd.dispatch((uint32_t)ceil((float)window.width / 16.0f), (uint32_t)ceil((float)window.height / 8.0f), 1);
}

void PrimaryPipeline::destroy(const Device& device) {
    /* Destroy the shader modules */
    device.device.destroyShaderModule(primary_shader);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
