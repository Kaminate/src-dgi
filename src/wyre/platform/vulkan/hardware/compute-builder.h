#pragma once

#include "../api.h"

namespace wyre {

using ShaderStage = vk::ShaderStageFlagBits;

/**
 * @brief Vulkan compute pipeline builder.
 */
class ComputeBuilder {
    /* Pipeline components */
    vk::PipelineShaderStageCreateInfo compute_stage{};

    /* Pipeline layout */
    std::vector<vk::PushConstantRange> push_constants {};
    std::vector<vk::DescriptorSetLayout> desc_sets {};

   public:
    ComputeBuilder();
    
    /** @brief Build the actual pipeline layout. */
    vk::ResultValue<vk::PipelineLayout> build_layout(const vk::Device device) const;
    /** @brief Build the actual graphics pipeline. */
    vk::ResultValue<vk::Pipeline> build_pipeline(const vk::Device device, const vk::PipelineLayout layout) const;

    /* Builder functions */
    
    /** @brief Add some push constants into the pipeline layout. */
    void add_push_constants(const size_t size, const size_t offset = 0);
    /** @brief Add a descriptor set layout into the pipeline layout. */
    void add_descriptor_set(const vk::DescriptorSetLayout layout);

    /** @brief Set shader entry point of the pipeline. */
    void set_shader_entry(const vk::ShaderModule& module, const char* entry_point);
};

}  // namespace wyre
