/**
 * @file pipelines/overlay.cpp
 * @brief Vulkan ImGui pass pipeline.
 */
#include "overlay.h"

#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>

#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"

namespace wyre {

OverlayPipeline::OverlayPipeline(Logger& logger, const Window& window, const Device& device) {

    { /* Create ImGui descriptor pool */
        const std::array<vk::DescriptorPoolSize, 11> sizes {
            vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 512),
            vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 512)
        };
        const vk::DescriptorPoolCreateInfo desc_pool_ci(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 512, sizes);
        const vk::ResultValue result = device.device.createDescriptorPool(desc_pool_ci);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create imgui descriptor pool.");
            return;
        }
        desc_pool = result.value;
    }

    /* Create the ImGui context */
    ImGui::CreateContext();
    window.init_imgui();

    /* Create the ImGui init info */
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = device.instance;
    init_info.PhysicalDevice = device.phy_device;
    init_info.Device = device.device;
    init_info.QueueFamily = device.qf_graphics;
    init_info.Queue = device.queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = desc_pool;
    init_info.RenderPass = VK_NULL_HANDLE;
    init_info.Subpass = 0;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = true;

    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    const VkFormat swapchain_fmt = (VkFormat)device.swapchain_fmt;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_fmt;

    /* Init ImGui and Font(s) */
    if (not ImGui_ImplVulkan_Init(&init_info)) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to init imgui.");
        return;
    }
    if (not ImGui_ImplVulkan_CreateFontsTexture()) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create imgui fonts and textures.");
        return;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized overlay pipeline.");
}

/**
 * @brief Push ImGui pipeline commands into the graphics command buffer.
 */
void OverlayPipeline::enqueue(const Window& window, const Device& device) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const RenderTarget& rt = device.get_rt();

    /* Imgui calculate rectangles and prepare for draw */
    ImGui::Render();

    /* Render target memory barrier */
    img::barrier(cmd, rt.img,
        /* Src */ img::PStage::eFragmentShader, img::Access::eNone, img::Layout::eColorAttachmentOptimal,
        /* Dst */ img::PStage::eFragmentShader, img::Access::eNone, img::Layout::eColorAttachmentOptimal);

    /* Define the render target attachment */
    vk::RenderingAttachmentInfoKHR attachment_info{};
    attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    attachment_info.loadOp = vk::AttachmentLoadOp::eLoad;
    attachment_info.imageView = rt.view;

    /* Rendering info */
    vk::RenderingInfoKHR rendering_info{};
    rendering_info.renderArea.extent = vk::Extent2D{window.width, window.height};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;
    rendering_info.layerCount = 1;

    /* Setup for rendering */
    cmd.beginRenderingKHR(&rendering_info, device.dldi); /* Start rendering */

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    cmd.endRenderingKHR(device.dldi); /* End rendering */
}

void OverlayPipeline::destroy(const Device& device) {
    ImGui_ImplVulkan_Shutdown();
    device.device.destroyDescriptorPool(desc_pool);
}

}  // namespace wyre
