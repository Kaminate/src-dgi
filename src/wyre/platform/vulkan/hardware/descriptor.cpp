#include "descriptor.h"

#include "../device.h"

namespace wyre {

void DescriptorSet::free(const Device& device) {
    /* Destroy descriptor set */
    device.device.destroyDescriptorSetLayout(layout);
    device.device.destroyDescriptorPool(pool);
}

void DescriptorSet::attach_constant_buffer(const Device& device, const uint32_t binding, vk::Buffer buffer, const uint32_t size) {
    /* Descriptor write info */
    const vk::DescriptorBufferInfo buffer_info(buffer, 0, size);
    const vk::WriteDescriptorSet write(set, binding, 0, vk::DescriptorType::eUniformBuffer, {}, buffer_info);

    device.device.updateDescriptorSets(write, {});
}

void DescriptorSet::attach_storage_buffer(const Device& device, const uint32_t binding, vk::Buffer buffer, const uint32_t size) {
    /* Descriptor write info */
    const vk::DescriptorBufferInfo buffer_info(buffer, 0, size);
    const vk::WriteDescriptorSet write(set, binding, 0, vk::DescriptorType::eStorageBuffer, {}, buffer_info);

    device.device.updateDescriptorSets(write, {});
}

void DescriptorSet::attach_image_sampler(
    const Device& device, const uint32_t binding, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout) {
    /* Descriptor write info */
    const vk::DescriptorImageInfo image_info(sampler, view, layout);
    const vk::WriteDescriptorSet write(set, binding, 0, vk::DescriptorType::eCombinedImageSampler, image_info);

    device.device.updateDescriptorSets(write, {});
}

void DescriptorSet::attach_storage_image(
    const Device& device, const uint32_t binding, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout) {
    /* Descriptor write info */
    const vk::DescriptorImageInfo image_info(sampler, view, layout);
    const vk::WriteDescriptorSet write(set, binding, 0, vk::DescriptorType::eStorageImage, image_info);

    device.device.updateDescriptorSets(write, {});
}

void DescriptorBuilder::add_binding(const uint32_t binding, const vk::DescriptorType type, const uint32_t count) {
    /* Define the binding */
    const vk::DescriptorSetLayoutBinding bind(binding, type, count);

    /* Save the binding */
    bindings.push_back(bind);
}

DescriptorSet DescriptorBuilder::build(const Device& device, vk::ShaderStageFlags stages) {
    /* Set the stage access flags */
    for (vk::DescriptorSetLayoutBinding& binding : bindings) {
        binding.stageFlags |= stages;
    }

    /* Setup the layout creation info */
    const vk::DescriptorSetLayoutCreateInfo info({}, bindings);
    DescriptorSet desc_set{};

    { /* Create the descriptor set layout */
        const vk::ResultValue result = device.device.createDescriptorSetLayout(info);
        if (result.result != vk::Result::eSuccess) {
            assert(false && "failed to build descriptor set!");
            return desc_set;
        }
        desc_set.layout = result.value;
    }

    { /* Create descriptor pool */
        const std::array<vk::DescriptorPoolSize, 3> sizes {
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 16),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 16),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 16)
        };
        const vk::DescriptorPoolCreateInfo desc_pool_ci({}, 1, sizes);
        const vk::ResultValue result = device.device.createDescriptorPool(desc_pool_ci);
        if (result.result != vk::Result::eSuccess) {
            assert(false && "failed to allocate descriptor pool!");
            return desc_set;
        }
        desc_set.pool = result.value;
    }

    { /* Allocate the descriptor set */
        const vk::DescriptorSetAllocateInfo desc_set_alloc(desc_set.pool, desc_set.layout);
        const vk::ResultValue result = device.device.allocateDescriptorSets(desc_set_alloc);
        if (result.result != vk::Result::eSuccess) {
            assert(false && "failed to allocate descriptor set!");
            return desc_set;
        }
        desc_set.set = result.value.front();
    }

    return desc_set;
}

}  // namespace wyre
