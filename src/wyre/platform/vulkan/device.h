/**
 * @file device.h
 * @brief Vulkan initialization & core render loop.
 */
#pragma once

#include <functional> /* std::function */

#include "wyre/defines.h"
#include "frame-data.h"
#include "hardware/descriptor.h"

#include "wyre/result.h" /* Result<T> */

namespace img {
struct RenderAttachment;
}

namespace wyre {

class Window;
class Logger;

/**
 * @brief Vulkan specific Device.
 * @warning This Device is not exposed to the user!
 */
class Device {
    /* Allow the engine to access private functions */
    friend class WyreEngine;

    Device() = default;
    ~Device() = default;

    /* Engine required functions */
    Result<void> init(Logger& logger, const Window& window);
    bool start_frame();
    void end_frame();
    Result<void> destroy();

    /** @brief Wait for the GPU to become idle, can be used before exiting the engine. */
    bool wait_idle() const { return queue.waitIdle() == vk::Result::eSuccess; };

   public:
    inline const FrameData& get_frame() const { return frames[fbi]; };
    inline const RenderTarget& get_rt() const { return targets[sci]; };

    /* Platform specific data */
    vk::Instance instance = nullptr;
    vk::detail::DispatchLoaderDynamic dldi = {};
    vk::PhysicalDevice phy_device = nullptr; /* Physical device. */
    vk::Device device = nullptr;             /* Logical device. */
    vk::Queue queue = nullptr;               /* Device queue. */
    int qf_graphics = -1, qf_present = -1;   /* Queue family index. */
    vk::CommandPool cmd_pool = nullptr;      /* Command pool, memory pool for command buffers. */
    vk::SurfaceKHR surface = nullptr;        /* Native video output surface. */
    vk::SwapchainKHR swapchain = nullptr;    /* Swapchain. */
    vk::Format swapchain_fmt = {};           /* Swapchain image format. */
    VmaAllocator allocator = nullptr;        /* Vulkan memory allocator. */
    vk::Fence imm_fence = nullptr;           /* Immediate submit fence. */
    vk::CommandBuffer imm_cmd = nullptr;     /* Immediate command buffer. */

    /* Descriptor pool with static lifetime. */
    vk::DescriptorPool static_desc_pool = nullptr;
    vk::DescriptorSetLayout static_desc_layout = nullptr;
    vk::Sampler nearest_sampler = nullptr;

    FrameData frames[BUFFERS] = {};
    RenderTarget targets[BUFFERS] = {};
    uint32_t fid = 0; /* Frame index */
    uint32_t fbi = 0; /* Frame Buffer Index (FBI) */
    uint32_t sci = 0; /* Swapchain image index */

#if DEBUG
    vk::DebugUtilsMessengerEXT debug_msgr = nullptr;
#endif

    /** @brief Get access to the Vulkan Memory Allocator instance. */
    inline VmaAllocator get_allocator() const { return allocator; };

    /**
     * @brief Queue some commands on the GPU to be enqueued immediately.
     */
    bool imm_submit(std::function<void(vk::CommandBuffer cmd)>&& commands) const;
};

}  // namespace wyre
