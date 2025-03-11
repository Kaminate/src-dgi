/**
 * @file image.h
 * @brief Vulkan image helper functions.
 */
#pragma once

#include "../api.h"

namespace wyre {
class Device;
}

namespace wyre::img {

/* Shortened names */
using Layout = vk::ImageLayout;
using Access = vk::AccessFlagBits;
using PStage = vk::PipelineStageFlagBits;
using UsageFlags = vk::ImageUsageFlags;
using Usage = vk::ImageUsageFlagBits;

/**
 * @brief Image transform data.
 */
struct Transform {
    img::Layout layout = img::Layout::eUndefined;
    img::PStage pipeline_stage = img::PStage::eNone;
    img::Access access = img::Access::eNone;

    Transform() = default;
    Transform(img::Layout layout, img::PStage pipeline_stage, img::Access access = img::Access::eNone);
};

/**
 * @brief Transform image from one format to another & apply a memory access barrier.
 *
 * @example 
 * img::barrier(cmd, image,
 *      Src img::Layout::eUndefined, img::PStage::eAllGraphics,
 *      Dst img::Layout::ePresentSrcKHR, img::PStage::eBottomOfPipe);
 */
void barrier(vk::CommandBuffer cmd, vk::Image image, img::PStage blocking, img::Layout src_layout, img::PStage blocked, img::Layout dst_layout);

/**
 * @brief Transform image from one format to another & apply a memory access barrier.
 */
void barrier(vk::CommandBuffer cmd, vk::Image image, img::PStage blocking, img::Access src_access, img::Layout src_layout, img::PStage blocked, img::Access dst_access, img::Layout dst_layout);

/**
 * @brief Rendering attachment, e.g. Albedo, Normal, Depth...
 */
struct RenderAttachment {
    vk::Image image {};
    VmaAllocation memory {};
    vk::ImageView view {};
    vk::Format format {};
    
    RenderAttachment() = default;

    /** @brief Free the attachment memory. */
    void free(const Device& device);

    /**
     * @brief Make a new rendering attachment, e.g. Albedo, Normal, Depth...
     * 
     * @param attachment The output attachment.
     */
    static bool make(const Device& device, RenderAttachment& attachment, vk::Extent2D size, vk::Format format, UsageFlags usage);
};

struct Texture2D {
    vk::Image image {};
    VmaAllocation memory {};
    vk::ImageView view {};
    vk::Format format {};
    uint32_t width = 0u, height = 0u;

    Texture2D() = default;
    
    /** @brief Free the attachment memory. */
    void free(const Device& device);
    
    /**
     * @brief Make a new texture.
     */
    static bool make(const Device& device, Texture2D& out_texture, vk::Extent2D size, vk::Format format, vk::ImageAspectFlags aspect, vk::ImageLayout layout, vk::ImageUsageFlags usage);
};

}  // namespace wyre::img
