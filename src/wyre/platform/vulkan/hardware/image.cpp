/**
 * @file image.cpp
 * @brief Vulkan image helper functions.
 */
#include "image.h"

#include "../device.h"

namespace wyre::img {

Transform::Transform(img::Layout layout, img::PStage pipeline_stage, img::Access access)
    : layout(layout), pipeline_stage(pipeline_stage), access(access) {}

void barrier(vk::CommandBuffer cmd, vk::Image image, img::PStage blocking, img::Layout src_layout, img::PStage blocked, img::Layout dst_layout) {
    constexpr uint32_t qf_ignored = vk::QueueFamilyIgnored;

    /* Simple color image is assumed */
    const vk::ImageSubresourceRange image_range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    /* Image transformation barrier (for sync) */
    const vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier(
        /* Masks: Source, Destination */
        img::Access::eNone, img::Access::eNone,
        /* Layout: Old, New */
        src_layout, dst_layout,
        /* Queue families */
        qf_ignored, qf_ignored,
        /* Image */
        image, image_range);

    /* Insert pipeline barrier command */
    cmd.pipelineBarrier(blocking, blocked, vk::DependencyFlags{0}, {}, {}, barrier);
}

void barrier(vk::CommandBuffer cmd, vk::Image image, img::PStage blocking, img::Access src_access, img::Layout src_layout, img::PStage blocked, img::Access dst_access, img::Layout dst_layout) {
    constexpr uint32_t qf_ignored = vk::QueueFamilyIgnored;

    /* Simple color image is assumed */
    const vk::ImageSubresourceRange image_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    /* Image transformation barrier (for sync) */
    const vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier(
        /* Masks: Source, Destination */
        src_access, dst_access,
        /* Layout: Old, New */
        src_layout, dst_layout,
        /* Queue families */
        qf_ignored, qf_ignored,
        /* Image */
        image, image_range);

    /* Insert pipeline barrier command */
    cmd.pipelineBarrier(blocking, blocked, vk::DependencyFlags{0}, {}, {}, barrier);
}

void RenderAttachment::free(const Device& device) {
    device.device.destroyImageView(view);
    vmaDestroyImage(device.get_allocator(), image, memory);
}

/**
 * @brief Make a new rendering attachment, e.g. Albedo, Normal, Depth...
 *
 * @param attachment The output attachment.
 */
bool RenderAttachment::make(const Device& device, RenderAttachment& attachment, vk::Extent2D size, vk::Format format, UsageFlags usage) {
    /* Figure out the correct aspect flags & image layout based on format & usage */
    vk::ImageAspectFlags aspect_flags{};
    vk::ImageLayout layout{};

    /* If this attachment is used as a color attachment */
    if (usage & vk::ImageUsageFlagBits::eColorAttachment) {
        aspect_flags = vk::ImageAspectFlagBits::eColor;
        layout = vk::ImageLayout::eColorAttachmentOptimal;
    }

    /* If this attachment is used as a depth/stencil attachment */
    if (usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
        aspect_flags = vk::ImageAspectFlagBits::eDepth;
        if ((VkFormat)format >= VK_FORMAT_D16_UNORM_S8_UINT) aspect_flags |= vk::ImageAspectFlagBits::eStencil;
        layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }

    /* If none of the aspect flags were set, something went wrong... */
    if (aspect_flags == vk::ImageAspectFlagBits::eNone) return false;

    attachment.format = format;

    /* Attachment image blueprint */
    vk::ImageCreateInfo image_ci{};
    image_ci.imageType = vk::ImageType::e2D;
    image_ci.format = format;
    image_ci.extent = vk::Extent3D(size, 1);
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = vk::SampleCountFlagBits::e1;
    image_ci.tiling = vk::ImageTiling::eOptimal;
    image_ci.usage = usage | vk::ImageUsageFlagBits::eSampled;
    const VkImageCreateInfo vma_image_ci = (VkImageCreateInfo)image_ci;

    /* Attachment memory blueprint */
    const VmaAllocationCreateInfo mem_ci{};

    VkImage out_image; /* Allocate the attachment */
    if (vmaCreateImage(device.get_allocator(), &vma_image_ci, &mem_ci, &out_image, &attachment.memory, nullptr) != VK_SUCCESS) return false;
    attachment.image = out_image;

    /* Attachment image view blueprint */
    vk::ImageViewCreateInfo view_ci{};
    view_ci.viewType = vk::ImageViewType::e2D;
    view_ci.format = format;
    view_ci.subresourceRange.aspectMask = aspect_flags;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = 1;
    view_ci.image = attachment.image;

    /* Try to create an image view for the attachment */
    const vk::ResultValue view_result = device.device.createImageView(view_ci);
    if (view_result.result != vk::Result::eSuccess) return false;
    attachment.view = view_result.value;

    return true;
}

void Texture2D::free(const Device& device) {
    device.device.destroyImageView(view);
    vmaDestroyImage(device.get_allocator(), image, memory);
}

bool Texture2D::make(const Device& device, Texture2D& out_texture, vk::Extent2D size, vk::Format format, vk::ImageAspectFlags aspect, vk::ImageLayout layout, vk::ImageUsageFlags usage) {
    out_texture.format = format;
    out_texture.width = size.width;
    out_texture.height = size.height;

    /* Texture blueprint */
    vk::ImageCreateInfo image_ci{};
    image_ci.imageType = vk::ImageType::e2D;
    image_ci.format = format;
    image_ci.extent = vk::Extent3D(size, 1);
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = vk::SampleCountFlagBits::e1;
    image_ci.tiling = vk::ImageTiling::eOptimal;
    image_ci.usage = usage;
    const VkImageCreateInfo vma_image_ci = (VkImageCreateInfo)image_ci;

    /* Texture memory blueprint */
    const VmaAllocationCreateInfo mem_ci{};

    VkImage out_image; /* Allocate */
    if (vmaCreateImage(device.get_allocator(), &vma_image_ci, &mem_ci, &out_image, &out_texture.memory, nullptr) != VK_SUCCESS) return false;
    out_texture.image = out_image;

    /* Image view blueprint */
    vk::ImageViewCreateInfo view_ci{};
    view_ci.viewType = vk::ImageViewType::e2D;
    view_ci.format = format;
    view_ci.subresourceRange.aspectMask = aspect;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = 1;
    view_ci.image = out_texture.image;

    /* Try to create an image view */
    const vk::ResultValue view_result = device.device.createImageView(view_ci);
    if (view_result.result != vk::Result::eSuccess) return false;
    out_texture.view = view_result.value;

    return true;
}

}  // namespace wyre::img
