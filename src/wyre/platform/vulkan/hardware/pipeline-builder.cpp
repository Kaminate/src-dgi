#include "pipeline-builder.h"

namespace wyre {

PipelineBuilder::PipelineBuilder() {
    /* Default rasterizer settings */
    rasterizer.depthClampEnable = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = false;
}

vk::ResultValue<vk::PipelineLayout> PipelineBuilder::build_layout(const vk::Device device) const {
    /* Pipeline layout blueprint */
    vk::PipelineLayoutCreateInfo pipeline_layout_ci{};
    pipeline_layout_ci.setPushConstantRanges(push_constants);
    pipeline_layout_ci.setSetLayouts(desc_sets);

    return device.createPipelineLayout(pipeline_layout_ci);
}

vk::ResultValue<vk::Pipeline> PipelineBuilder::build_pipeline(const vk::Device device, const vk::PipelineLayout layout) const {
    /* Pipeline create info chained with dynamic rendering info */
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfoKHR> chain{};

    /* Pipeline sections */
    const vk::PipelineVertexInputStateCreateInfo vertex_input({}, vertex_bindings, vertex_attributes);
    const vk::PipelineViewportStateCreateInfo viewport_state({}, viewports, scissors);
    const vk::PipelineDynamicStateCreateInfo dynamic_state({}, dynamic_states);
    const vk::PipelineColorBlendStateCreateInfo colorblend_state({}, false, vk::LogicOp::eClear, colorblend_attachments);

    /* Pipeline blueprint */
    vk::GraphicsPipelineCreateInfo& pipeline_ci = chain.get<vk::GraphicsPipelineCreateInfo>();
    pipeline_ci.stageCount = shader_stages.size();
    pipeline_ci.pStages = shader_stages.data();
    pipeline_ci.pVertexInputState = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pViewportState = &viewport_state;
    pipeline_ci.pRasterizationState = &rasterizer;
    pipeline_ci.pDynamicState = &dynamic_state;
    pipeline_ci.pMultisampleState = &multisample_state;
    pipeline_ci.pColorBlendState = &colorblend_state;
    pipeline_ci.layout = layout;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = nullptr;
    pipeline_ci.basePipelineIndex = -1;

    /* Dynamic rendering extension */
    vk::PipelineRenderingCreateInfoKHR& pipeline_render_ci = chain.get<vk::PipelineRenderingCreateInfoKHR>();
    pipeline_render_ci.setColorAttachmentFormats(color_attachments);
    pipeline_ci.renderPass = nullptr; /* Using dynamic rendering extension */

    return device.createGraphicsPipeline(nullptr, pipeline_ci, nullptr);
}

/* Builder functions */

void PipelineBuilder::add_push_constants(const vk::ShaderStageFlags stages, const size_t size, const size_t offset) {
    push_constants.emplace_back(stages, (uint32_t)offset, (uint32_t)size);
}

void PipelineBuilder::add_descriptor_set(const vk::DescriptorSetLayout layout) {
    desc_sets.push_back(layout);
}

void PipelineBuilder::add_color_attachment(const vk::Format format) { color_attachments.emplace_back(std::move(format)); }

void PipelineBuilder::add_viewport(
    const float x, const float y, const float w, const float h, const float min_depth, const float max_depth) {
    viewports.emplace_back(x, y, w, h, min_depth, max_depth);
}

void PipelineBuilder::add_scissor(const int x, const int y, const int w, const int h) {
    scissors.emplace_back(vk::Offset2D{x, y}, vk::Extent2D{(uint32_t)w, (uint32_t)h});
}

void PipelineBuilder::add_shader_stage(const ShaderStage stage, const vk::ShaderModule& module, const char* entry_point) {
    shader_stages.emplace_back(vk::PipelineShaderStageCreateInfo({}, stage, module, entry_point));
}

void PipelineBuilder::add_dynamic_state(const vk::DynamicState state) { dynamic_states.emplace_back(std::move(state)); }

void PipelineBuilder::add_vertex_binding(const vk::VertexInputBindingDescription binding) {
    vertex_bindings.emplace_back(binding);
}

void PipelineBuilder::add_vertex_attributes(const vk::VertexInputAttributeDescription* attributes, const size_t count) {
    for (size_t i = 0; i < count; ++i) vertex_attributes.push_back(attributes[i]);
}

void PipelineBuilder::set_primitive_topology(const vk::PrimitiveTopology topology) { input_assembly.topology = topology; }

void PipelineBuilder::add_basic_colorblend_attachment(const bool blend, const vk::ColorComponentFlags write_mask) {
    colorblend_attachments.emplace_back(blend, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, write_mask);
}

void PipelineBuilder::set_polygon_mode(const vk::PolygonMode mode) { rasterizer.polygonMode = mode; }

}  // namespace wyre
