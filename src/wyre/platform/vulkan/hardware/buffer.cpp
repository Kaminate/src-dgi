/**
 * @file buffer.cpp
 * @brief Vulkan buffer helper functions.
 */
#include "buffer.h"

#include "../device.h"

namespace wyre::buf {

BufferParams::BufferParams(buf::Size size, buf::UsageFlags usage, buf::ShareMode share_mode)
    : size(size), usage(usage), share_mode(share_mode) {}

AllocParams::AllocParams(VmaMemoryUsage usage) : usage(usage) {}

AllocParams::AllocParams(VmaAllocationCreateFlags flags, VmaMemoryUsage usage) : flags(flags), usage(usage) {}

void Buffer::free(const Device& device) { vmaDestroyBuffer(device.get_allocator(), buffer, memory); }

/**
 * @brief Allocate a new buffer.
 */
bool alloc(const Device& device, buf::Buffer& buffer, const BufferParams buf_params, const AllocParams alloc_params, bool fill) {
    /* Buffer blueprint (no flags) */
    const VkBufferCreateInfo buf_ci = vk::BufferCreateInfo({}, buf_params.size, buf_params.usage, buf_params.share_mode);

    /* Memory blueprint */
    const VmaAllocationCreateInfo mem_ci{.flags = alloc_params.flags, .usage = alloc_params.usage};

    /* Create the buffer & allocate it using VMA */
    VkBuffer buf;
    VmaAllocation mem;
    if (vmaCreateBuffer(device.get_allocator(), &buf_ci, &mem_ci, &buf, &mem, nullptr) != VK_SUCCESS) return false;

    /* Return the buffer with its allocation */
    buffer = buf::Buffer(buf, mem, buf_params.size);
    if (fill && buf_params.usage & vk::BufferUsageFlagBits::eTransferDst) buf::fill(device, 0x00, buffer, buffer.size);
    return true;
}

/**
 * @brief Allocate a new buffer, and upload data into it using a staging buffer.
 *
 * @param buffer The output buffer.
 * @param usage Flags for how the buffer will be used.
 * @param data A pointer to the data.
 * @param size The size of the data in bytes.
 */
bool alloc_upload(const Device& device, buf::Buffer& buffer, buf::UsageFlags usage, const void* data, const buf::Size size) {
    const VmaAllocator allocator = device.get_allocator();

    /* Allocation parameters */
    const buf::AllocParams stage_ci {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};
    const buf::AllocParams buffer_ci {};

    /* Buffer usage flags */
    const vk::BufferUsageFlags stage_usage = buf::Usage::eTransferSrc;
    const vk::BufferUsageFlags buffer_usage = usage | buf::Usage::eTransferDst;

    /* Create the staging buffer & send vertex data to the GPU */
    buf::Buffer stage_buffer{};
    if (!buf::alloc(device, stage_buffer, {size, stage_usage}, stage_ci, false)) return false;
    if (!buf::copy_raw(device, stage_buffer, 0, size, data)) return false;
    
    /* Create the vertex buffer & perform a GPU copy from the staging buffer */
    if (!buf::alloc(device, buffer, {size, buffer_usage}, buffer_ci)) return false;
    if (!buf::copy(device, stage_buffer, buffer, size)) return false;

    stage_buffer.free(device); /* Free the staging buffer */
    return true;
}

/**
 * @brief Copy data from CPU memory directly into a buffer 1:1.
 *
 * @param buffer The buffer to copy into. (requires HOST_ACCESS flag)
 * @param src Source memory to copy.
 */
bool copy_raw(const Device& device, const buf::Buffer& buffer, buf::Size offset, buf::Size size, const void* src) {
    return vmaCopyMemoryToAllocation(device.get_allocator(), src, buffer.memory, offset, size) == VK_SUCCESS;
}

/**
 * @brief Copy data from a GPU buffer into another GPU buffer. (with respect to formats)
 *
 * @param src Source buffer.
 * @param dst Destination buffer.
 * @param size Number of bytes to copy.
 */
bool copy(const Device& device, const buf::Buffer& src, const buf::Buffer& dst, buf::Size size) {
    const vk::Buffer buf_src = src.buffer, buf_dst = dst.buffer;
    return device.imm_submit([&buf_src, &buf_dst, size](vk::CommandBuffer cmd) {
        /* Copy from source buffer into destination buffer */
        cmd.copyBuffer(buf_src, buf_dst, vk::BufferCopy(0, 0, size));
    });
}

/**
 * @brief Fill the GPU buffer with a repeated 32 bit value.
 */
bool fill(const Device& device, const uint32_t val, const buf::Buffer& dst, buf::Size size) {
    const vk::Buffer buf_dst = dst.buffer;
    return device.imm_submit([&buf_dst, size, val](vk::CommandBuffer cmd) {
        /* Fill the destination buffer */
        cmd.fillBuffer(buf_dst, 0, size, val);
    });
}

/**
 * @brief Upload data from the CPU to a GPU buffer using a staging buffer.
 */
bool upload(const Device& device, const buf::Buffer& dst, const void* data, const buf::Size size) {
    const VmaAllocator allocator = device.get_allocator();

    /* Allocation parameters */
    const buf::AllocParams stage_ci {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};
    const buf::AllocParams buffer_ci {};

    /* Buffer usage flags */
    const vk::BufferUsageFlags stage_usage = buf::Usage::eTransferSrc;

    /* Create the staging buffer & send vertex data to the GPU */
    buf::Buffer stage_buffer{};
    if (!buf::alloc(device, stage_buffer, {size, stage_usage}, stage_ci)) return false;
    if (!buf::copy_raw(device, stage_buffer, 0, size, data)) return false;
    
    /* Perform a GPU copy from the staging buffer */
    if (!buf::copy(device, stage_buffer, dst, size)) return false;

    stage_buffer.free(device); /* Free the staging buffer */
    return true;
}

/**
 * @brief Extract an unsigned 32 bit integer from a GPU buffer.
 * 
 * @param src Source buffer.
 * @param offset Byte offset in the source buffer.
 */
bool extract_u32(const Device& device, const buf::Buffer& src, buf::Size offset, uint32_t& dst) { 
    return vmaCopyAllocationToMemory(device.get_allocator(), src.memory, offset, &dst, sizeof(uint32_t)) == VK_SUCCESS; 
}

/**
 * @brief Set the memory of a GPU buffer to all zeros.
 */
bool clear(const Device& device, const buf::Buffer& buffer) { 
    void* host_ptr = nullptr;
    if (vmaMapMemory(device.get_allocator(), buffer.memory, &host_ptr) != VK_SUCCESS) return false; 
    memset(host_ptr, 0x00, buffer.size);
    vmaUnmapMemory(device.get_allocator(), buffer.memory);
    return vmaFlushAllocation(device.get_allocator(), buffer.memory, 0u, buffer.size) == VK_SUCCESS; 
}

/**
 * @brief Add a memory sync barrier to a buffer.
 */
void barrier(vk::CommandBuffer cmd, buf::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size, buf::PStage blocking, buf::Access src_access, buf::PStage blocked, buf::Access dst_access) {
    constexpr uint32_t qf_ignored = vk::QueueFamilyIgnored;

    /* Buffer memory barrier (for sync) */
    const vk::BufferMemoryBarrier barrier = vk::BufferMemoryBarrier(
        /* Masks: Source, Destination */
        src_access, dst_access,
        /* Queue families */
        qf_ignored, qf_ignored,
        /* Buffer */
        buffer.buffer, offset, size);

    /* Insert pipeline barrier command */
    cmd.pipelineBarrier(blocking, blocked, vk::DependencyFlags{0}, {}, barrier, {});
}

}  // namespace wyre::buf
