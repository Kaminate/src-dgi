#pragma once

#include "../api.h"

namespace wyre {

class Device;

/**
 * @brief Vulkan descriptor set instance.
 */
struct DescriptorSet {
    vk::DescriptorSet set{};
    vk::DescriptorSetLayout layout{};
    vk::DescriptorPool pool{};

    /** @brief Free the buffer memory. */
    void free(const Device& device);

    /** @brief Attach a constant buffer to a given binding slot. */
    void attach_constant_buffer(const Device& device, const uint32_t binding, vk::Buffer buffer, const uint32_t size);

    /** @brief Attach a storage buffer to a given binding slot. */
    void attach_storage_buffer(const Device& device, const uint32_t binding, vk::Buffer buffer, const uint32_t size);

    /** @brief Attach a image sampler combo to a given binding slot. */
    void attach_image_sampler(const Device& device, const uint32_t binding, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout);
    
    /** @brief Attach a storage image to a given binding slot. */
    void attach_storage_image(const Device& device, const uint32_t binding, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout);
};

/**
 * @brief Vulkan descriptor set builder.
 */
class DescriptorBuilder {
    std::vector<vk::DescriptorSetLayoutBinding> bindings{};

   public:
    DescriptorBuilder() = default;

    /* Non-copyable */
    DescriptorBuilder(const DescriptorBuilder&) = delete;
    DescriptorBuilder& operator=(const DescriptorBuilder&) = delete;

    /** @brief Add a binding to this descriptor set. */
    void add_binding(const uint32_t binding, const vk::DescriptorType type, const uint32_t count = 1);

    /** @brief Build the descriptor set. */
    DescriptorSet build(const Device& device, vk::ShaderStageFlags stages);
};

}  // namespace wyre
