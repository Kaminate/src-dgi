#pragma once

#include "api.h"

#include <glm/glm.hpp>

#include "hardware/image.h"      /* RenderAttachment */
#include "hardware/descriptor.h" /* DescriptorSet */
#include "hardware/buffer.h"     /* Buffer */

namespace wyre {

struct RenderTarget {
    /* Image view to the swapchain image for this frame */
    vk::ImageView view = nullptr;
    /* Swapchain image */
    vk::Image img = nullptr;
};

/** @brief View parameters for rendering. */
struct RenderView {
    /* View matrix. */
    glm::mat4 view{};
    /* Projection matrix. */
    glm::mat4 proj{};
    /* Inverse view matrix. */
    glm::mat4 inv_view{};
    /* Inverse projection matrix. */
    glm::mat4 inv_proj{};
    /* World-space origin of the view. */
    glm::vec3 origin{};
    /* Field of view in radians. */
    float fov = 0.0f;
};

/**
 * @brief Data for rendering a single frame.
 */
struct FrameData {
    /* Graphics Command Buffer, used to store all draw commands for this frame. */
    vk::CommandBuffer gcb = nullptr;
    /* Constant buffer for camera state. */
    buf::Buffer render_view{};
    /* Rendering attachments. */
    img::RenderAttachment albedo{};
    img::RenderAttachment normal_depth{}; /* rgb = normal, a = depth */
    /* Rendering attachments descriptor sets. */
    wyre::DescriptorSet attach_render_desc{};
    wyre::DescriptorSet attach_store_desc{};
    /* In flight fence. */
    vk::Fence flight_fence = nullptr;

    /* Image acquisition semaphore. */
    vk::Semaphore image_acquired = nullptr;
    /* Render completion semaphore. (used for presenting) */
    vk::Semaphore render_complete = nullptr;
};

}  // namespace wyre
