/**
 * @file buffer.h
 * @brief Vulkan buffer helper functions.
 */
#pragma once

#include "../api.h"

namespace wyre {
class Device;
}

namespace wyre::buf {

/* Shortened names */
using Usage = vk::BufferUsageFlagBits;
using UsageFlags = vk::BufferUsageFlags;
using Size = vk::DeviceSize;
using ShareMode = vk::SharingMode;
using Access = vk::AccessFlagBits;
using PStage = vk::PipelineStageFlagBits;

/**
 * @brief Buffer instance with memory instance.
 */
struct Buffer {
    vk::Buffer buffer{};
    VmaAllocation memory{};
    buf::Size size = 0u;

    Buffer() = default;
    Buffer(vk::Buffer buffer, VmaAllocation memory, buf::Size size) : buffer(buffer), memory(memory), size(size) {};

    /** @brief Free the buffer memory. */
    void free(const Device& device);
};

/**
 * @brief Buffer creation parameters.
 */
struct BufferParams {
    buf::Size size{};
    buf::UsageFlags usage{};
    buf::ShareMode share_mode = buf::ShareMode::eExclusive;

    BufferParams() = default;
    BufferParams(buf::Size size, buf::UsageFlags usage, buf::ShareMode share_mode = buf::ShareMode::eExclusive);
};

/**
 * @brief Buffer allocation parameters.
 */
struct AllocParams {
    VmaAllocationCreateFlags flags{};
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO;

    AllocParams() = default;
    AllocParams(VmaMemoryUsage usage);
    AllocParams(VmaAllocationCreateFlags flags, VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO);
};

/**
 * @brief Allocate a new buffer.
 */
bool alloc(const Device& device, buf::Buffer& buffer, const BufferParams buf_params, const AllocParams alloc_params = AllocParams(), bool fill = true);

/**
 * @brief Allocate a new buffer, and upload data into it using a staging buffer.
 *
 * @param buffer The output buffer.
 * @param usage Flags for how the buffer will be used.
 * @param data A pointer to the data.
 * @param size The size of the data in bytes.
 */
bool alloc_upload(const Device& device, buf::Buffer& buffer, buf::UsageFlags usage, const void* data, const buf::Size size);

/**
 * @brief Copy data from CPU memory directly into a buffer 1:1.
 *
 * @param buffer The buffer to copy into. (requires HOST_ACCESS flag)
 * @param src Source memory to copy.
 */
bool copy_raw(const Device& device, const buf::Buffer& buffer, buf::Size offset, buf::Size size, const void* src);

/**
 * @brief Copy data from a GPU buffer into another GPU buffer. (with respect to formats)
 *
 * @param src Source buffer.
 * @param dst Destination buffer.
 * @param size Number of bytes to copy.
 */
bool copy(const Device& device, const buf::Buffer& src, const buf::Buffer& dst, buf::Size size);

/**
 * @brief Fill the GPU buffer with a repeated 32 bit value.
 */
bool fill(const Device& device, const uint32_t val, const buf::Buffer& dst, buf::Size size);

/**
 * @brief Upload data from the CPU to a GPU buffer using a staging buffer.
 */
bool upload(const Device& device, const buf::Buffer& dst, const void* data, const buf::Size size);

/**
 * @brief Extract an unsigned 32 bit integer from a GPU buffer.
 * 
 * @param src Source buffer.
 * @param offset Byte offset in the source buffer.
 */
bool extract_u32(const Device& device, const buf::Buffer& src, buf::Size offset, uint32_t& dst);

/**
 * @brief Set the memory of a GPU buffer to all zeros.
 */
bool clear(const Device& device, const buf::Buffer& buffer);

/**
 * @brief Add a memory sync barrier to a buffer.
 */
void barrier(vk::CommandBuffer cmd, buf::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size, buf::PStage blocking, buf::Access src_access, buf::PStage blocked, buf::Access dst_access);

}  // namespace wyre::buf
