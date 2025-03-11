/**
 * @file device.cpp
 * @brief Vulkan initialization & core render loop.
 */
#include "device.h"

#include <algorithm> /* std::min, std::max */

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE 1
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "api.h"

#include "hardware/image.h"
#include "hardware/device.h"
#include "hardware/ext.h"

#include "wyre/core/system/window.h"
#include "wyre/core/system/log.h"

namespace wyre {

/* Custom vulkan debug msg callback */
inline VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* cb_data, void* logger) {
    /* Find out what log level we should use */
    LogLevel level = LogLevel::INFO;
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: level = LogLevel::WARNING; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: level = LogLevel::CRITICAL; break;
    }

    ((Logger*)logger)->log(LogGroup::GRAPHICS_API, level, cb_data->pMessage);
    return VK_FALSE;
}

/**
 * @brief Device initialization.
 */
Result<void> Device::init(Logger& logger, const Window& window) {
    /* Instance creation information */
    const vk::ApplicationInfo app_info("wyre", 1, "wyre", 1, VK_API_VERSION_1_3);
    std::vector<const char*> i_extensions = get_sdl_extensions();
#if DEBUG
    i_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    const std::vector<const char*> i_layers = get_optimal_validation_layers(vk::enumerateInstanceLayerProperties().value);

#if DEBUG
    /* Debug messenger create info */
    const vk::DebugUtilsMessengerCreateInfoEXT debug_msgr_ci =
        vk::DebugUtilsMessengerCreateInfoEXT({}, vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            vk_debug_callback, (void*)&logger);

    /* Instance create info */
    const vk::InstanceCreateInfo instance_chain_ci({}, &app_info, i_layers, i_extensions);
    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instance_chain(instance_chain_ci, debug_msgr_ci);

    const vk::InstanceCreateInfo& instance_ci = instance_chain.get<vk::InstanceCreateInfo>();
#else
    const vk::InstanceCreateInfo instance_ci({}, &app_info, {}, i_extensions);
#endif

    { /* Create the Vulkan instance */
        const vk::ResultValue result = vk::createInstance(instance_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to init vulkan instance.");
        instance = result.value;
    }

    dldi = vk::detail::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);

#if DEBUG
    { /* Create the vulkan debug messenger */
        const vk::ResultValue result = instance.createDebugUtilsMessengerEXT(debug_msgr_ci, nullptr, dldi);
        if (result.result != vk::Result::eSuccess) return Err("failed to create debug messenger.");
        debug_msgr = result.value;
        logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "created vulkan debug messenger.");
    }
#endif

    { /* Select a physical device */
        const Result<vk::PhysicalDevice> result = get_physical_device(instance);
        if (result.is_err()) return Err(result.unwrap_err());
        phy_device = result.unwrap();
    }

    { /* Debug info */
        const vk::PhysicalDeviceProperties props = phy_device.getProperties();
        const std::string name = props.deviceName;
        const uint32_t major = vk::apiVersionMajor(props.apiVersion);
        const uint32_t minor = vk::apiVersionMinor(props.apiVersion);
        const uint32_t patch = vk::apiVersionPatch(props.apiVersion);

        logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "selected physical device: %s (v%u.%u.%u)", name.c_str(), major, minor, patch);
    }

    /* Create the native video output surface */
    if (window.create_surface(instance, surface) == false) {
        return Err("failed to create native video output surface.");
    }

    /* Find a graphics queue family */
    const std::vector<vk::QueueFamilyProperties> queuefamily_props = phy_device.getQueueFamilyProperties();
    for (int i = 0; i < (int)queuefamily_props.size(); ++i) {
        const vk::QueueFamilyProperties& props = queuefamily_props[i];

        /* Find the first properties which support graphics */
        if (props.queueFlags & vk::QueueFlagBits::eGraphics) {
            qf_graphics = i;
            break;
        }
    }

    if (qf_graphics == -1) {
        return Err("failed to find graphics queue family properties.");
    }

    { /* Check if the graphics queue family also supports present */
        const vk::ResultValue result = phy_device.getSurfaceSupportKHR((uint32_t)qf_graphics, surface);
        if (result.result != vk::Result::eSuccess || !result.value) return Err("graphics queue doesn't support present.");
        qf_present = qf_graphics;
    } /* TODO: in case of failure above, we can look for a different queue family that supports both. */

    float queue_priority = 0.0f;
    const vk::DeviceQueueCreateInfo device_queue_ci(vk::DeviceQueueCreateFlags(), (uint32_t)qf_graphics, 1, &queue_priority);
    { /* Create the logical device */
        vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceDynamicRenderingFeaturesKHR> chain;

        /* Device creation info */
        const std::vector<const char*> layers = i_layers;
        vk::DeviceCreateInfo& device_ci = chain.get<vk::DeviceCreateInfo>();
        device_ci.setPEnabledExtensionNames(DEVICE_EXT);
        device_ci.setPEnabledLayerNames(layers);
        device_ci.setQueueCreateInfos(device_queue_ci);

        /* Dynamic rendering feature */
        vk::PhysicalDeviceDynamicRenderingFeaturesKHR& dynamic_feature = chain.get<vk::PhysicalDeviceDynamicRenderingFeaturesKHR>();
        dynamic_feature.dynamicRendering = true;

        const vk::ResultValue result = phy_device.createDevice(device_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to create logical device.");
        device = result.value;
    }

    /* Get the device queue */
    queue = device.getQueue(qf_graphics, 0);

    { /* Create graphics command pool */
        const vk::CommandPoolCreateInfo cmd_pool_ci(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, (uint32_t)qf_graphics);
        const vk::ResultValue result = device.createCommandPool(cmd_pool_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to create command pool.");
        cmd_pool = result.value;
    }

    /* Create a command buffer for each frame buffer */
    for (size_t i = 0; i < BUFFERS; ++i) {
        const vk::CommandBufferAllocateInfo cmd_buf_ai(cmd_pool, vk::CommandBufferLevel::ePrimary, 1);
        const vk::ResultValue result = device.allocateCommandBuffers(cmd_buf_ai);
        if (result.result != vk::Result::eSuccess) return Err("failed to allocate graphics command buffer.");
        frames[i].gcb = result.value.front();
    }

    { /* Create an immediate submit command buffer */
        const vk::CommandBufferAllocateInfo cmd_buf_ai(cmd_pool, vk::CommandBufferLevel::ePrimary, 1);
        const vk::ResultValue result = device.allocateCommandBuffers(cmd_buf_ai);
        if (result.result != vk::Result::eSuccess) return Err("failed to allocate immediate command buffer.");
        imm_cmd = result.value.front();
    }

    std::vector<vk::SurfaceFormatKHR> formats = {};
    { /* Get the supported formats for our native video output surface */
        const vk::ResultValue result = phy_device.getSurfaceFormatsKHR(surface);
        if (result.result != vk::Result::eSuccess) return Err("failed to get formats for native video output surface.");
        formats = result.value;
    }

    if (formats.empty()) {
        return Err("no formats for native video output surface.");
    }

    /* Simply grab the first available format, if undefined choose RGBA8 unorm */
    const vk::Format first_format = formats[0].format;
    swapchain_fmt = (first_format == vk::Format::eUndefined) ? vk::Format::eB8G8R8A8Unorm : first_format;

    vk::SurfaceCapabilitiesKHR capabilities = {};
    { /* Get the native video output surface capabilities */
        const vk::ResultValue result = phy_device.getSurfaceCapabilitiesKHR(surface);
        if (result.result != vk::Result::eSuccess) return Err("failed to get native video output surface capabilities.");
        capabilities = result.value;
    }

    /* Find the extent of the swapchain */
    vk::Extent2D swapchain_extent = {};
    const uint32_t max_val = std::numeric_limits<uint32_t>::max();
    if (capabilities.currentExtent.width == max_val) {
        /* If the surface size is undefined, we can pick our own preferred resolution */
        const vk::Extent2D min = capabilities.minImageExtent;
        const vk::Extent2D max = capabilities.maxImageExtent;
        swapchain_extent.width = std::min(std::max(window.width, min.width), max.width);
        swapchain_extent.height = std::min(std::max(window.height, min.height), max.height);
    } else {
        /* If the surface size is defined, the swapchain size MUST match */
        swapchain_extent = capabilities.currentExtent;
        /* TODO: throw a warning here, I feel like this is usually not a good case... */
    }

    /* Swapchain image count */
    if (capabilities.maxImageCount < BUFFERS) {
        return Err("native video output surface does not support image count.");
    }

    /* Swapchain present mode */
    const vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;

    /* We want to present without any special transformations (identity) */
    const vk::SurfaceTransformFlagBitsKHR preferred_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    const bool transform_available = (bool)(capabilities.supportedTransforms & preferred_transform);
    const vk::SurfaceTransformFlagBitsKHR transform = transform_available ? preferred_transform : capabilities.currentTransform;

    /* No alpha blending between frames */
    const vk::CompositeAlphaFlagBitsKHR comp_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    const vk::ColorSpaceKHR color_space = vk::ColorSpaceKHR::eSrgbNonlinear;

    vk::SwapchainCreateInfoKHR swapchain_ci(vk::SwapchainCreateFlagsKHR(), surface, BUFFERS, swapchain_fmt, color_space, swapchain_extent, 1,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive, {}, transform, comp_alpha,
        present_mode, true, nullptr);

    /* Catch the case where the graphics and present queue family aren't the same */
    const uint32_t qf_indices[2] = {(uint32_t)qf_graphics, (uint32_t)qf_present};
    if (qf_graphics != qf_present) {
        /* Create the swapchain with image sharing mode as concurrent */
        swapchain_ci.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchain_ci.queueFamilyIndexCount = 2;
        swapchain_ci.pQueueFamilyIndices = qf_indices;
    }

    { /* Create the swapchain */
        const vk::ResultValue result = device.createSwapchainKHR(swapchain_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to create swapchain.");
        swapchain = result.value;
    }

    { /* Retrieve the swapchain image resources */
        const vk::ResultValue result = device.getSwapchainImagesKHR(swapchain);
        if (result.result != vk::Result::eSuccess) return Err("failed retrieve swapchain images.");
        const std::vector<vk::Image> target_images = result.value;

        /* Create an image view (render target) for each frame buffer */
        const vk::ImageSubresourceRange range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        vk::ImageViewCreateInfo target_view_ci({}, {}, vk::ImageViewType::e2D, swapchain_fmt, {}, range);
        for (size_t i = 0; i < BUFFERS; ++i) {
            target_view_ci.image = target_images[i];
            const vk::ResultValue result = device.createImageView(target_view_ci);
            if (result.result != vk::Result::eSuccess) return Err("failed to create swapchain image view.");
            targets[i] = {result.value, target_images[i]};
        }
    }

    { /* Create in sync primitives for each frame */
        for (size_t i = 0; i < BUFFERS; ++i) {
            const vk::ResultValue result = device.createFence({vk::FenceCreateFlagBits::eSignaled});
            if (result.result != vk::Result::eSuccess) return Err("failed to create render fence.");
            frames[i].flight_fence = result.value;
            const vk::ResultValue result2 = device.createSemaphore({});
            if (result2.result != vk::Result::eSuccess) return Err("failed to create semaphore.");
            frames[i].image_acquired = result2.value;
            const vk::ResultValue result3 = device.createSemaphore({});
            if (result3.result != vk::Result::eSuccess) return Err("failed to create semaphore.");
            frames[i].render_complete = result3.value;
        }
    }

    { /* Create immediate sync fence */
        const vk::ResultValue result = device.createFence({vk::FenceCreateFlagBits::eSignaled});
        if (result.result != vk::Result::eSuccess) return Err("failed to create immediate fence.");
        imm_fence = result.value;
    }

    /* Create the Vulkan Memory Allocator */
    VmaAllocatorCreateInfo allocator_ci = {};
    allocator_ci.physicalDevice = phy_device;
    allocator_ci.device = device;
    allocator_ci.instance = instance;
    allocator_ci.flags = {};
    if (vmaCreateAllocator(&allocator_ci, &allocator) != VK_SUCCESS) {
        return Err("failed to create vulkan memory allocator.");
    }

    /* Create the rendering attachments */
    const vk::Extent2D win_size{window.width, window.height};
    for (size_t i = 0; i < BUFFERS; ++i) {
        /* Render view */
        const buf::AllocParams alloc_ci{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};
        if (!buf::alloc(*this, frames[i].render_view, {sizeof(RenderView), buf::Usage::eUniformBuffer | buf::Usage::eTransferDst}, alloc_ci)) {
            return Err("failed to allocate render view buffer.");
        }
        const RenderView default_renderview {};
        buf::copy_raw(*this, frames[i].render_view, 0u, frames[i].render_view.size, &default_renderview);
        /* Albedo attachment */
        const bool r_albedo = img::RenderAttachment::make(*this, frames[i].albedo, win_size, vk::Format::eR8G8B8A8Unorm, img::Usage::eColorAttachment | img::Usage::eStorage);
        if (r_albedo == false) return Err("failed to create albedo render attachment.");
        /* Normal attachment */
        const bool r_normal = img::RenderAttachment::make(*this, frames[i].normal_depth, win_size, vk::Format::eR32G32B32A32Sfloat, img::Usage::eColorAttachment | img::Usage::eStorage);
        if (r_normal == false) return Err("failed to create normal render attachment.");
    }

    { /* Create static descriptor pool */
        const std::array<vk::DescriptorPoolSize, 3> sizes {
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 128),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 128),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 128)
        };
        const vk::DescriptorPoolCreateInfo desc_pool_ci({}, 32, sizes);
        const vk::ResultValue result = device.createDescriptorPool(desc_pool_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to create static descriptor pool.");
        static_desc_pool = result.value;
    }

    { /* Create a nearest sampler */
        const vk::SamplerCreateInfo sampler_ci{};
        const vk::ResultValue result = device.createSampler(sampler_ci);
        if (result.result != vk::Result::eSuccess) return Err("failed to create nearest sampler.");
        nearest_sampler = result.value;
    }

    { /* Create the render attachment descriptors */
        DescriptorBuilder render_builder {}, store_builder {};
        /* Bindings */
        render_builder.add_binding(0, vk::DescriptorType::eUniformBuffer);
        render_builder.add_binding(1, vk::DescriptorType::eCombinedImageSampler);
        render_builder.add_binding(2, vk::DescriptorType::eCombinedImageSampler);

        /* Create the rendering descriptor sets */
        for (size_t i = 0; i < BUFFERS; ++i) {
            wyre::DescriptorSet& desc_set = frames[i].attach_render_desc;

            desc_set = render_builder.build(*this, vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment);
            desc_set.attach_constant_buffer(*this, 0, frames[i].render_view.buffer, frames[i].render_view.size);
            desc_set.attach_image_sampler(*this, 1, frames[i].albedo.view, nearest_sampler, vk::ImageLayout::eShaderReadOnlyOptimal);
            desc_set.attach_image_sampler(*this, 2, frames[i].normal_depth.view, nearest_sampler, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
        
        /* Bindings */
        store_builder.add_binding(0, vk::DescriptorType::eUniformBuffer);
        store_builder.add_binding(1, vk::DescriptorType::eStorageImage);
        store_builder.add_binding(2, vk::DescriptorType::eStorageImage);
        
        /* Create the storage descriptor sets */
        for (size_t i = 0; i < BUFFERS; ++i) {
            wyre::DescriptorSet& desc_set = frames[i].attach_store_desc;

            desc_set = store_builder.build(*this, vk::ShaderStageFlagBits::eCompute);
            desc_set.attach_constant_buffer(*this, 0, frames[i].render_view.buffer, frames[i].render_view.size);
            desc_set.attach_storage_image(*this, 1, frames[i].albedo.view, nearest_sampler, vk::ImageLayout::eGeneral);
            desc_set.attach_storage_image(*this, 2, frames[i].normal_depth.view, nearest_sampler, vk::ImageLayout::eGeneral);
        }
    }

    return Ok();
}

/**
 * @brief Setup the current frame for rendering.
 */
bool Device::start_frame() {
    /* Select the next frame buffer */
    fbi = fid % BUFFERS;

    /* Get this frames graphics command buffer */
    const vk::CommandBuffer& cmd = get_frame().gcb;
    const vk::Fence& fence = get_frame().flight_fence;
    const vk::Semaphore& image_acquired = get_frame().image_acquired;

    /* Wait for the frame to be available */
    if (device.waitForFences(fence, true, UINT64_MAX) != vk::Result::eSuccess) return false;
    if (device.resetFences(fence) != vk::Result::eSuccess) return false;

    /* Acquire swapchain image */
    const vk::ResultValue result = device.acquireNextImageKHR(swapchain, UINT64_MAX, image_acquired);
    if (result.result != vk::Result::eSuccess) return false;
    sci = result.value;
    const RenderTarget& rt = get_rt();

    /* Signal that this command buffer will only be submitted *once* */
    if (cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit}) != vk::Result::eSuccess) return false;

    /* Clear color & range */
    const vk::ClearColorValue clear_value = vk::ClearColorValue({1.0f, 0.0f, 0.0f, 1.0f});
    const vk::ImageSubresourceRange clear_range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    /* Transition image into transfer destination format */
    img::barrier(cmd, rt.img,
        /* Src */ img::PStage::eBottomOfPipe, img::Layout::eUndefined,
        /* Dst */ img::PStage::eBottomOfPipe, img::Layout::eTransferDstOptimal);

    /* Queue a clear command */
    cmd.clearColorImage(rt.img, vk::ImageLayout::eTransferDstOptimal, clear_value, {clear_range});
    return true;
}

/**
 * @brief Finish rendering the current frame. (present)
 */
void Device::end_frame() {
    /* Get this frames graphics command buffer */
    const vk::CommandBuffer& cmd = get_frame().gcb;
    const vk::Fence& fence = get_frame().flight_fence;
    const vk::Semaphore& image_acquired = get_frame().image_acquired;
    const vk::Semaphore& render_complete = get_frame().render_complete;
    const RenderTarget& rt = get_rt();

    /* Transition image into presentable format */
    img::barrier(cmd, rt.img,
        /* Src */ img::PStage::eTopOfPipe, img::Layout::eUndefined,
        /* Dst */ img::PStage::eBottomOfPipe, img::Layout::ePresentSrcKHR);

    if (cmd.end() != vk::Result::eSuccess) return; /* Stop recording commands */

    /* Submit work to the GPU (waits for image acquired, signals when render is complete) */
    const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    const vk::SubmitInfo info = vk::SubmitInfo(image_acquired, wait_stage, cmd, render_complete);
    if (queue.submit(info, fence) != vk::Result::eSuccess) return;

    /* Present (waits for render to complete) */
    const vk::PresentInfoKHR present_info(render_complete, swapchain, sci);
    if (queue.presentKHR(present_info) != vk::Result::eSuccess) return;
    
    /* Select the next frame buffer */
    fid += 1;
}

/**
 * @brief Queue some commands on the GPU to be enqueued immediately.
 */
bool Device::imm_submit(std::function<void(vk::CommandBuffer cmd)>&& commands) const {
    /* Try to reset the immediate fence */
    if (device.resetFences(imm_fence) != vk::Result::eSuccess) return false;

    /* Signal that this command buffer will only be submitted *once* */
    if (imm_cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit}) != vk::Result::eSuccess) return false;

    commands(imm_cmd); /* <- Submit commands */

    if (imm_cmd.end() != vk::Result::eSuccess) return false; /* Stop recording commands */

    /* Submit immediate work to the GPU */
    const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo info{};
    info.setCommandBuffers(imm_cmd);
    if (queue.submit(info, imm_fence) != vk::Result::eSuccess) return false;

    /* Wait for the work on the GPU to complete */
    if (device.waitForFences(imm_fence, true, UINT64_MAX) != vk::Result::eSuccess) return false;
    if (queue.waitIdle() != vk::Result::eSuccess) return false;

    return true;
}

/**
 * @brief Cleanup device resources.
 */
Result<void> Device::destroy() {
    /* Wait for the current queue to finish */
    if (queue.waitIdle() != vk::Result::eSuccess) return Err("failed to wait for gpu idle.");

    /* Destroy the frame data & render targets */
    for (size_t i = 0; i < BUFFERS; ++i) {
        device.destroyImageView(targets[i].view);
        /* Sync primitives */
        device.destroyFence(frames[i].flight_fence);
        device.destroySemaphore(frames[i].image_acquired);
        device.destroySemaphore(frames[i].render_complete);
        frames[i].render_view.free(*this);
        /* Render attachments */
        frames[i].albedo.free(*this);
        frames[i].normal_depth.free(*this);
        /* Render attachment descriptor sets */
        frames[i].attach_render_desc.free(*this);
        frames[i].attach_store_desc.free(*this);
    }

    /* Destroy the Vulkan Memory Allocator */
    vmaDestroyAllocator(allocator);

    /* Destroy static descriptor set */
    device.destroyDescriptorSetLayout(static_desc_layout);
    device.destroyDescriptorPool(static_desc_pool);

    /* Destroy nearest sampler */
    device.destroySampler(nearest_sampler);

    /* Destroy immediate resources */
    device.destroyFence(imm_fence);

    /* Destroy the swapchain and native video output surface */
    device.destroySwapchainKHR(swapchain);
    instance.destroySurfaceKHR(surface);

    /* Free the command buffer pool */
    device.destroyCommandPool(cmd_pool);

    /* Destroy the logical device */
    device.destroy();

#if DEBUG
    instance.destroyDebugUtilsMessengerEXT(debug_msgr, nullptr, dldi);
#endif

    /* Destroy the vulkan instance */
    instance.destroy();

    return Ok();
}

}  // namespace wyre
