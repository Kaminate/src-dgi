#pragma once

#include "../api.h"

namespace wyre {

using ShaderStage = vk::ShaderStageFlagBits;

/**
 * @brief Vulkan pipeline builder.
 */
class PipelineBuilder {
    /* Pipeline components */
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages{};
    std::vector<vk::DynamicState> dynamic_states{};
    /* Vertex input */
    std::vector<vk::VertexInputBindingDescription> vertex_bindings{};
    std::vector<vk::VertexInputAttributeDescription> vertex_attributes{};
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    vk::PipelineMultisampleStateCreateInfo multisample_state{};
    std::vector<vk::PipelineColorBlendAttachmentState> colorblend_attachments{};
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    /* Viewports & scissors */
    std::vector<vk::Viewport> viewports {};
    std::vector<vk::Rect2D> scissors {};
    std::vector<vk::Format> color_attachments{};

    /* Pipeline layout */
    std::vector<vk::PushConstantRange> push_constants {};
    std::vector<vk::DescriptorSetLayout> desc_sets {};

   public:
    PipelineBuilder();
    
    /** @brief Build the actual pipeline layout. */
    vk::ResultValue<vk::PipelineLayout> build_layout(const vk::Device device) const;
    /** @brief Build the actual graphics pipeline. */
    vk::ResultValue<vk::Pipeline> build_pipeline(const vk::Device device, const vk::PipelineLayout layout) const;

    /* Builder functions */
    
    /** @brief Add some push constants into the pipeline layout. */
    void add_push_constants(const vk::ShaderStageFlags stages, const size_t size, const size_t offset = 0);
    /** @brief Add a descriptor set layout into the pipeline layout. */
    void add_descriptor_set(const vk::DescriptorSetLayout layout);

    /** @brief Add a color attachment to the pipeline. */
    void add_color_attachment(const vk::Format format);

    /** @brief Add a viewport to the pipeline. */
    void add_viewport(const float x, const float y, const float w, const float h, const float min_depth, const float max_depth);
    /** @brief Add a scissor to the pipeline. */
    void add_scissor(const int x, const int y, const int w, const int h);

    /** @brief Add a shader stage to the pipeline. */
    void add_shader_stage(const ShaderStage stage, const vk::ShaderModule& module, const char* entry_point);
    /** @brief Add some dynamic state to the pipeline. */
    void add_dynamic_state(const vk::DynamicState state);

    /** @brief Add a vertex binding to the pipeline. */
    void add_vertex_binding(const vk::VertexInputBindingDescription binding);
    /** @brief Add vertex attributes to the pipeline. */
    void add_vertex_attributes(const vk::VertexInputAttributeDescription* attributes, const size_t count);

    /** @brief Set the primitive topology of the pipeline. */
    void set_primitive_topology(const vk::PrimitiveTopology topology);

    /** @brief Add a basic color blend attachment to the pipeline. */
    void add_basic_colorblend_attachment(const bool blend,
        const vk::ColorComponentFlags write_mask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    /** @brief Set the polygon mode of the pipeline rasterizer. */
    void set_polygon_mode(const vk::PolygonMode mode);
};

}  // namespace wyre
