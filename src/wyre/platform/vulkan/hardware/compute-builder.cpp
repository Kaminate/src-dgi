#include "compute-builder.h"

namespace wyre {

ComputeBuilder::ComputeBuilder() {
}

vk::ResultValue<vk::PipelineLayout> ComputeBuilder::build_layout(const vk::Device device) const {
    /* Pipeline layout blueprint */
    vk::PipelineLayoutCreateInfo pipeline_layout_ci{};
    pipeline_layout_ci.setPushConstantRanges(push_constants);
    pipeline_layout_ci.setSetLayouts(desc_sets);

    return device.createPipelineLayout(pipeline_layout_ci);
}

vk::ResultValue<vk::Pipeline> ComputeBuilder::build_pipeline(const vk::Device device, const vk::PipelineLayout layout) const {
    /* Pipeline blueprint */
    vk::ComputePipelineCreateInfo pipeline_ci({}, compute_stage, layout);
    vk::PipelineCache cache(nullptr);

    return device.createComputePipeline(VK_NULL_HANDLE, pipeline_ci, nullptr);
}

/* Builder functions */

void ComputeBuilder::add_push_constants(const size_t size, const size_t offset) {
    push_constants.emplace_back(vk::ShaderStageFlagBits::eCompute, (uint32_t)offset, (uint32_t)size);
}

void ComputeBuilder::add_descriptor_set(const vk::DescriptorSetLayout layout) {
    desc_sets.push_back(layout);
}

void ComputeBuilder::set_shader_entry(const vk::ShaderModule& module, const char* entry_point) {
    compute_stage = vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, module, entry_point);
}

}  // namespace wyre
