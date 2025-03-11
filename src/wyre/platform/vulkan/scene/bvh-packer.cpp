/**
 * @file stages/bvh-packer.cpp
 * @brief Vulkan bvh packer stage. (sends BVH to the GPU)
 */
#include "bvh-packer.h"

#include "vulkan/device.h"

#include "wyre/core/scene/bvh.h" /* scene::Bvh */
#include "wyre/core/system/log.h"

namespace wyre {

using namespace scene;

const uint32_t BUF_SIZE = 1056818 * 2;

SceneBvhPacker::SceneBvhPacker(Logger& logger, const Device& device) {
    const buf::AllocParams alloc_ci {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};
    /* Allocate the BVH nodes buffer */
    if (!buf::alloc(device, bvh_nodes, {sizeof(Bvh::GPUNode) * BUF_SIZE, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate bvh nodes buffer.");
        return;
    }
    /* Allocate the BVH primitives buffer */
    if (!buf::alloc(device, bvh_prims, {sizeof(Triangle) * BUF_SIZE, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate bvh vertex buffer.");
        return;
    }
    /* Allocate the BVH normals buffer */
    if (!buf::alloc(device, bvh_norms, {sizeof(Normals) * BUF_SIZE, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate bvh normal buffer.");
        return;
    }

    DescriptorBuilder bvh_desc_builder {};
    /* Constant buffer(s) */
    bvh_desc_builder.add_binding(0, vk::DescriptorType::eStorageBuffer);
    bvh_desc_builder.add_binding(1, vk::DescriptorType::eStorageBuffer);
    bvh_desc_builder.add_binding(2, vk::DescriptorType::eStorageBuffer);

    /* Build the BVH descriptor set */
    bvh_desc = bvh_desc_builder.build(device, vk::ShaderStageFlagBits::eCompute);
    bvh_desc.attach_storage_buffer(device, 0, bvh_nodes.buffer, sizeof(Bvh::GPUNode) * BUF_SIZE);
    bvh_desc.attach_storage_buffer(device, 1, bvh_prims.buffer, sizeof(Triangle) * BUF_SIZE);
    bvh_desc.attach_storage_buffer(device, 2, bvh_norms.buffer, sizeof(Normals) * BUF_SIZE);
}

/**
 * @brief Pack the scene BVH and upload it to the GPU buffer.
 */
void SceneBvhPacker::package(const Device& device, const scene::Bvh& bvh) {
    /* TEMP: only upload BVH once for now. */
    static uint32_t pre_nodes_used = 0;
    static uint32_t pre_prim_count = 0;

    /* Update the BVH nodes buffer */
    if (bvh.gpu_nodes && pre_nodes_used != bvh.nodes_used) { 
        buf::upload(device, bvh_nodes, bvh.gpu_nodes, sizeof(Bvh::GPUNode) * bvh.nodes_used);
        pre_nodes_used = bvh.nodes_used;
    }
    if (bvh.prims && pre_prim_count != bvh.prim_count) { 
        buf::upload(device, bvh_prims, bvh.prims, sizeof(Triangle) * bvh.prim_count);
        buf::upload(device, bvh_norms, bvh.norms, sizeof(Normals) * bvh.prim_count);
        pre_prim_count = bvh.prim_count;
    }
}

void SceneBvhPacker::destroy(const Device& device) {
    /* Free BVH data */
    bvh_desc.free(device);
    bvh_nodes.free(device);
    bvh_prims.free(device);
    bvh_norms.free(device);
}

}  // namespace wyre
