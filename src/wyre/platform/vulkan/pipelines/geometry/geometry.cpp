/**
 * @file pipelines/geometry.cpp
 * @brief Vulkan geometry pipeline.
 */
#include "geometry.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/pipeline-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"

namespace wyre {

/* Uber shaders */
const char* UBER_SHADER = "assets/shaders/uber.slang.spv";

struct PushConstants {
    glm::mat4 mvp;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;

    static inline vk::VertexInputBindingDescription bind_desc() {
        return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    };

    static inline std::array<vk::VertexInputAttributeDescription, 2> attr_desc() {
        std::array<vk::VertexInputAttributeDescription, 2> desc{};
        desc[0] = vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0);
        desc[1] = vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, sizeof(glm::vec3));
        return desc;
    };
};

const Vertex TRIANGLE[3] = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 1.0f}}, {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}, {{-0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}}};

const Vertex CUBE[] = {
    /* -z face */
    {{0, 0, 0}, {0.0f, 0.0f}}, {{0, 1, 0}, {0.0f, 1.0f}}, {{1, 1, 0}, {1.0f, 1.0f}}, {{1, 0, 0}, {1.0f, 0.0f}},

    /* +y face */
    {{0, 1, 0}, {0.0f, 0.0f}}, {{0, 1, 1}, {0.0f, 1.0f}}, {{1, 1, 1}, {1.0f, 1.0f}}, {{1, 1, 0}, {1.0f, 0.0f}},

    /* +x face */
    {{1, 0, 0}, {0.0f, 0.0f}}, {{1, 1, 0}, {0.0f, 1.0f}}, {{1, 1, 1}, {1.0f, 1.0f}}, {{1, 0, 1}, {1.0f, 0.0f}},

    /* -y face */
    {{0, 0, 1}, {0.0f, 0.0f}}, {{0, 0, 0}, {0.0f, 1.0f}}, {{1, 0, 0}, {1.0f, 1.0f}}, {{1, 0, 1}, {1.0f, 0.0f}},

    /* -x face */
    {{0, 0, 1}, {0.0f, 0.0f}}, {{0, 1, 1}, {0.0f, 1.0f}}, {{0, 1, 0}, {1.0f, 1.0f}}, {{0, 0, 0}, {1.0f, 0.0f}},

    /* +z face */
    {{1, 0, 1}, {0.0f, 0.0f}}, {{1, 1, 1}, {0.0f, 1.0f}}, {{0, 1, 1}, {1.0f, 1.0f}}, {{0, 0, 1}, {1.0f, 0.0f}}};

const uint16_t CUBE_IND[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16,
    18, 19, 20, 21, 22, 20, 22, 23};

GeometryPipeline::GeometryPipeline(Logger& logger, const Window& window, const Device& device) {
    /* Load the uber fragment & vertex shader modules */
    uber_shader = shader::from_file(device.device, UBER_SHADER).expect("failed to load uber shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded geometry shader modules.");

    PipelineBuilder builder{};
    /* Shader stages */
    builder.add_shader_stage(ShaderStage::eVertex, uber_shader, "main");
    builder.add_shader_stage(ShaderStage::eFragment, uber_shader, "main");
    /* Dynamic state */
    builder.add_dynamic_state(vk::DynamicState::eViewport);
    builder.add_dynamic_state(vk::DynamicState::eScissor);
    /* Vertex input */
    builder.add_vertex_binding(Vertex::bind_desc());
    builder.add_vertex_attributes(Vertex::attr_desc().data(), 2);
    builder.set_primitive_topology(vk::PrimitiveTopology::eTriangleList);
    /* Color blend mode */
    builder.add_basic_colorblend_attachment(false);
    builder.add_color_attachment(vk::Format::eR8G8B8A8Unorm);
    /* Rasterizer */
    builder.add_viewport(0, 0, window.width, window.height, 0.0f, 1.0f);
    builder.add_scissor(0, 0, window.width, window.height);
    builder.set_polygon_mode(vk::PolygonMode::eFill);
    /* Add the vertex stage push constants */
    builder.add_push_constants(vk::ShaderStageFlagBits::eVertex, sizeof(PushConstants));

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create geometry pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create geometry graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized geometry pipeline.");

    /* Create vertices & indices buffers */
    const VmaAllocator& vma = device.allocator;
    if (!buf::alloc_upload(device, vertex_buffer, buf::Usage::eVertexBuffer, CUBE, sizeof(CUBE))) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate vertex buffer.");
        return;
    }
    if (!buf::alloc_upload(device, index_buffer, buf::Usage::eIndexBuffer, CUBE_IND, sizeof(CUBE_IND))) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to allocate index buffer.");
        return;
    }
}

/**
 * @brief Push geometry pipeline commands into the graphics command buffer.
 */
void GeometryPipeline::enqueue(const Window& window, const Device& device) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const img::RenderAttachment& albedo = device.get_frame().albedo;

    /* Transition albedo attachment into color attachment format */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eAllGraphics, img::Layout::eUndefined,
        /* Dst */ img::PStage::eAllGraphics, img::Layout::eColorAttachmentOptimal);

    /* Define the render target albedo attachment */
    vk::RenderingAttachmentInfoKHR attachment_info{};
    attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    attachment_info.clearValue.color = vk::ClearColorValue({0.05f, 0.05f, 0.05f, 1.0f});
    attachment_info.imageView = albedo.view;

    /* Viewport & scissor */
    const vk::Viewport viewport(0, 0, window.width, window.height, 0.0f, 1.0f);
    const vk::Rect2D scissor({0, 0}, {window.width, window.height});

    /* Rendering info */
    vk::RenderingInfoKHR rendering_info{};
    rendering_info.renderArea.extent = vk::Extent2D{window.width, window.height};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;
    rendering_info.layerCount = 1;
    /* TODO: Use depth buffer & enable depth testing */

    /* TEMP: Rotating cube over time... */
    static float time = 0.0f;
    time += 0.016f;

    /* Create view and projection matrix */
    const float aspect = (float)window.width / (float)window.height;
    const glm::mat4 proj = glm::perspective(1.57f /* 90deg */, aspect, .1f, 100.f);
    const glm::mat4 view = glm::lookAt(glm::vec3(0, -2, -3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    const glm::mat4 vp = proj * view * glm::toMat4(glm::angleAxis(time, glm::vec3(0, 1, 0)));
    const PushConstants pc = PushConstants{.mvp = vp};

    /* Setup for rendering */
    cmd.beginRenderingKHR(&rendering_info, device.dldi); /* Start rendering */
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    cmd.bindVertexBuffers(0, {vertex_buffer.buffer}, {0});
    cmd.bindIndexBuffer(index_buffer.buffer, 0, vk::IndexType::eUint16);
    cmd.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants), &pc);

    /* Draw */
    cmd.setViewport(0, {viewport});
    cmd.setScissor(0, {scissor});
    cmd.drawIndexed(sizeof(CUBE_IND) / 2, 1, 0, 0, 0, device.dldi);

    cmd.endRenderingKHR(device.dldi); /* End rendering */
}

void GeometryPipeline::destroy(const Device& device) {
    vertex_buffer.free(device);
    index_buffer.free(device);

    /* Destroy the uber fragment & vertex shader modules */
    device.device.destroyShaderModule(uber_shader);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
