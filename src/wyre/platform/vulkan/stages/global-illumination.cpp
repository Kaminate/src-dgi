/**
 * @file stages/global-illumination.cpp
 * @brief Vulkan global illumination rendering stage.
 */
#include "global-illumination.h"

#include <imgui.h>

#include "vulkan/hardware/descriptor.h" /* DescriptorSet */
#include "vulkan/hardware/debug.h" /* begin_label() */

#include "vulkan/pipelines/global-illumination/surfel-count.h" /* SurfelCountPipeline */
#include "vulkan/pipelines/global-illumination/surfel-prefix.h" /* SurfelPrefixPipeline */
#include "vulkan/pipelines/global-illumination/surfel-accel.h" /* SurfelAccelerationPipeline */
#include "vulkan/pipelines/global-illumination/surfel-spawn.h" /* SurfelSpawnPipeline */
#include "vulkan/pipelines/global-illumination/surfel-gather.h" /* SurfelGatherPipeline */
#include "vulkan/pipelines/global-illumination/surfel-merge.h" /* SurfelMergePipeline */
#include "vulkan/pipelines/global-illumination/surfel-composite.h" /* SurfelCompositePipeline */
#include "vulkan/pipelines/global-illumination/surfel-recycle.h" /* SurfelRecyclePipeline */
#include "vulkan/pipelines/global-illumination/surfel-draw.h" /* SurfelDrawPipeline */
#include "vulkan/pipelines/global-illumination/surfel-heatmap.h" /* SurfelHeatmapPipeline */
#include "vulkan/pipelines/global-illumination/ground-truth.h" /* GroundTruthPipeline */
#include "vulkan/pipelines/global-illumination/surfels.h"

#include "wyre/core/graphics/device.h"
#include "wyre/core/system/log.h"

namespace wyre {

GIStage::GIStage(Logger& logger, const Window& window, const Device& device, const DescriptorSet& bvh)
    : cascade_dummy(device), /* <- This sucks... but whatever... */
      surfel_count_pipeline(*new SurfelCountPipeline(logger, device, cascade_dummy)),
      surfel_prefix_pipeline(*new SurfelPrefixPipeline(logger, device, cascade_dummy)),
      surfel_accel_pipeline(*new SurfelAccelerationPipeline(logger, device, cascade_dummy)),
      surfel_spawn_pipeline(*new SurfelSpawnPipeline(logger, device, cascade_dummy)),
      surfel_gather_pipeline(*new SurfelGatherPipeline(logger, device, bvh, cascade_dummy)),
      surfel_merge_pipeline(*new SurfelMergePipeline(logger, device, cascade_dummy)),
      surfel_composite_pipeline(*new SurfelCompositePipeline(logger, window, device, cascade_dummy)),
      surfel_recycle_pipeline(*new SurfelRecyclePipeline(logger, device, cascade_dummy)),
      surfel_debug_pipeline(*new SurfelDrawPipeline(logger, window, device, cascade_dummy)),
      surfel_heatmap_pipeline(*new SurfelHeatmapPipeline(logger, window, device, cascade_dummy)),
      ground_truth_pipeline(*new GroundTruthPipeline(logger, device, window, bvh)) {
    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        cascades[i] = SurfelCascadeResources(device);
    }

    init_resources(logger, device);
}

void GIStage::init_resources(Logger& logger, const Device& device) {
    /* Allocate Surfel Cascades */
    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        if (cascades[i].alloc(device, cascade_params, i) == false) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate surfel cascade resources.");
            return;
        }
    }
}

/**
 * @brief Push GI stage commands into the graphics command buffer.
 */
void GIStage::enqueue(const Window& window, const Device& device, const DescriptorSet& bvh) {
    if (ground_truth) {
        /* Ground truth pass */
        ground_truth_pipeline.enqueue(window, device, bvh);
        return;
    }

    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const img::RenderAttachment& albedo = device.get_frame().albedo;
    const img::RenderAttachment& normal_depth = device.get_frame().normal_depth;

    /* Transform the albedo & normal depth gbuffer to read only optimal layout */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eShaderReadOnlyOptimal);
    img::barrier(cmd, normal_depth.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eShaderReadOnlyOptimal);

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        /* Buffer memory barrier on the Surfel Hash Grid buffer */
        buf::barrier(cmd, cascades[i].surfel_grid, 0, cascades[i].surfel_grid.size, 
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    }

    debug::begin_label(device, "Surfel Spawning", {0.035f, 0.573f, 0.408f});
    
    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel spawn pass */
        surfel_spawn_pipeline.enqueue(window, device, cascade);
    }

    /* Place a memory barrier on the albedo output */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eShaderReadOnlyOptimal,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    img::barrier(cmd, normal_depth.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eShaderReadOnlyOptimal,
        /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);

    debug::end_label(device);
    debug::begin_label(device, "Surfel Hash Counting", {0.898f, 0.6f, 0.969f});

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        /* Clear the Surfel Hash Grid structure */
        cmd.fillBuffer(cascades[i].surfel_grid.buffer, 0u, cascades[i].surfel_grid.size, 0x00);
    }

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        /* Buffer memory barrier on the Surfel Hash Grid buffer */
        buf::barrier(cmd, cascades[i].surfel_grid, 0, cascades[i].surfel_grid.size, 
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
        /* Buffer memory barrier on the Surfel stack buffer */
        buf::barrier(cmd, cascades[i].surfel_stack, 0, cascades[i].surfel_stack.size, 
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    }

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel counting passes */
        surfel_count_pipeline.enqueue(device, cascade);
    }

    debug::end_label(device);
    debug::begin_label(device, "Surfel Hash Prefix Sum", {0.898f, 0.6f, 0.969f});

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel prefix sum passes */
        surfel_prefix_pipeline.enqueue(device, cascade);
    }

    debug::end_label(device);
    debug::begin_label(device, "Surfel Hash Insertion", {0.898f, 0.6f, 0.969f});

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];
        /* Buffer memory barrier on the Surfel hash grid buffer */
        buf::barrier(cmd, cascade.surfel_grid, 0, cascade.surfel_grid.size, 
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
            
        /* Get the updated number of live Surfels from the GPU stack buffer */
        cascade.update_surfel_count(device, cascade_params);
    }

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel maintenance passes */
        surfel_accel_pipeline.enqueue(device, cascade);
    }

    debug::end_label(device);

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        img::barrier(cmd, cascades[i].surfel_rad.image,
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eUndefined,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral);
        img::barrier(cmd, cascades[i].surfel_merge.image,
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eUndefined,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral);
        
        /* Buffer memory barrier on the Surfel posr buffer */
        buf::barrier(cmd, cascades[i].surfel_posr, 0, cascades[i].surfel_posr.size, 
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead);
    }
    
    debug::begin_label(device, "Surfel Gathering", {0.251f, 0.753f, 0.341f});

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel gathering pass */
        surfel_gather_pipeline.enqueue(window, device, bvh, cascade);
    }

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        /* Place a memory barrier on the surfel radiance cache */
        img::barrier(cmd, cascades[i].surfel_rad.image,
            /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
            /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);

        // /* Copy the radiance cache to the merged radiance cache for merging */
        // cmd.copyImage(cascades[i].surfel_rad.image, vk::ImageLayout::eGeneral, cascades[i].surfel_merge.image, vk::ImageLayout::eGeneral, {
        //     vk::ImageCopy(
        //         vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {},
        //         vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {},
        //         vk::Extent3D(cascades[i].surfel_rad.width, cascades[i].surfel_rad.height, 1)
        //     )
        // });
        
        // /* Place a memory barrier on the surfel merged radiance cache */
        // img::barrier(cmd, cascades[i].surfel_merge.image,
        //     /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        //     /* Dst */ img::PStage::eComputeShader, img::Access::eShaderRead, img::Layout::eGeneral);
    }

    debug::end_label(device);
    debug::begin_label(device, "Surfel Merging", {0.302f, 0.671f, 0.969f});

    for (int i = CASCADE_COUNT - 2; i >= 0; --i) {
        SurfelCascadeResources& src_cascade = cascades[i + 1u];
        SurfelCascadeResources& dst_cascade = cascades[i];
        
        /* Surfel merge pass */
        surfel_merge_pipeline.enqueue(device, src_cascade, dst_cascade);
    }

    debug::end_label(device);
    debug::begin_label(device, "Surfel Composite", {0.576f, 0.596f, 0.690f});

    /* Surfel composite pass */
    surfel_composite_pipeline.enqueue(window, device, cascades[0]);

    debug::end_label(device);
    debug::begin_label(device, "Surfel Debug", {0.878f, 0.192f, 0.192f});

    /* DEBUGGING */
    if (heatmap) surfel_heatmap_pipeline.enqueue(window, device, cascades[debug_cascade_index]);
    if (direct_draw) surfel_debug_pipeline.enqueue(window, device, cascades[debug_cascade_index]);

    debug::end_label(device);
    debug::begin_label(device, "Surfel Recycling", {0.310f, 0.447f, 0.988f});

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];
        /* Buffer memory barrier on the surfel norw buffer */
        buf::barrier(cmd, cascade.surfel_norw, 0, cascade.surfel_norw.size, 
            /* Src */ buf::PStage::eComputeShader, buf::Access::eShaderWrite,
            /* Dst */ buf::PStage::eComputeShader, buf::Access::eShaderRead);
    }

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        SurfelCascadeResources& cascade = cascades[i];

        /* Surfel recycling pass */
        surfel_recycle_pipeline.enqueue(device, cascade);
    }
    
    debug::end_label(device);
}

void GIStage::update_params(Logger& logger, const Device& device) {
    free_resources(device);
    init_resources(logger, device);
}

void GIStage::destroy(const Device& device) {
    surfel_count_pipeline.destroy(device);
    delete &surfel_count_pipeline;
    surfel_prefix_pipeline.destroy(device);
    delete &surfel_prefix_pipeline;
    surfel_accel_pipeline.destroy(device);
    delete &surfel_accel_pipeline;
    surfel_spawn_pipeline.destroy(device);
    delete &surfel_spawn_pipeline;
    surfel_gather_pipeline.destroy(device);
    delete &surfel_gather_pipeline;
    surfel_merge_pipeline.destroy(device);
    delete &surfel_merge_pipeline;
    surfel_composite_pipeline.destroy(device);
    delete &surfel_composite_pipeline;
    surfel_recycle_pipeline.destroy(device);
    delete &surfel_recycle_pipeline;

    surfel_debug_pipeline.destroy(device);
    delete &surfel_debug_pipeline;
    surfel_heatmap_pipeline.destroy(device);
    delete &surfel_heatmap_pipeline;
    ground_truth_pipeline.destroy(device);
    delete &ground_truth_pipeline;

    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        cascades[i].free(device);
    }
    cascade_dummy.free(device); /* <- I want to get rid of this dummy... */
}

void GIStage::free_resources(const Device& device) {
    while (device.queue.waitIdle() != vk::Result::eSuccess);

    /* Free the Surfel buffers */
    for (uint32_t i = 0u; i < CASCADE_COUNT; ++i) {
        cascades[i].free_buffers(device);
    }
}

}  // namespace wyre
