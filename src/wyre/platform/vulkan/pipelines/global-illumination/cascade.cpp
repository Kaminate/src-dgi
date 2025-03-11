#include "cascade.h"

#include "surfels.h" /* SAS_CELL_CAPACITY */

#include "vulkan/device.h"

namespace wyre {

/* 262.144 probes */
constexpr uint32_t MAX_SURFEL_COUNT = 1u << 18u;

/* TODO: Perhaps branch factors could become parameters as well? */
/* Branch factor: Sx/Ax */
constexpr uint32_t SPATIAL_FACTOR = 4; /* Spatial branch factor. */
constexpr uint32_t ANGULAR_FACTOR = 4; /* Angular branch factor. */

/* Get the spatial scaling value for a cascade. */
inline uint32_t spatial_scale(const uint32_t cascade_index) { return (uint32_t)pow(SPATIAL_FACTOR, cascade_index); }

/* WARN: Only works if the branch factor can be divided by 2. */
/* Get half the spatial scaling value for a cascade. */
inline uint32_t half_spatial_scale(const uint32_t cascade_index) { return (uint32_t)pow(SPATIAL_FACTOR >> 1u, cascade_index); }

/* Get the angular scaling value for a cascade. */
inline uint32_t angular_scale(const uint32_t cascade_index) { return (uint32_t)pow(ANGULAR_FACTOR, cascade_index); }

/* WARN: Only works if the branch factor can be divided by 2. */
/* Get half the angular scaling value for a cascade. */
inline uint32_t half_angular_scale(const uint32_t cascade_index) { return (uint32_t)pow(ANGULAR_FACTOR >> 1u, cascade_index); }

SurfelCascadeParameters::SurfelCascadeParameters() {
    /* 512 * 512 is based on the SEGMENT size in the prefix sum */
    c0_grid_capacity = (512u * 512u) - 1u; /* -1 because the last element has to be empty for prefix sum */
    c0_grid_scale = 70.0f;
    cell_capacity = SAS_CELL_CAPACITY;
    c0_memory_width = 4u; /* 4x4 = 16 intervals */
    c0_probe_capacity = MAX_SURFEL_COUNT;
    c0_probe_radius = 0.002f;
    max_solid_angle = 0.005f;
}

uint32_t SurfelCascadeParameters::get_memory_width(const uint32_t cascade_index) const 
{ return c0_memory_width * half_angular_scale(cascade_index); }
uint32_t SurfelCascadeParameters::get_probe_capacity(const uint32_t cascade_index) const
{ return c0_probe_capacity / spatial_scale(cascade_index); }
uint32_t SurfelCascadeParameters::get_grid_capacity(const uint32_t cascade_index) const
{ return c0_grid_capacity; /* / half_spatial_scale(cascade_index); */ }

SurfelCascadeResources::SurfelCascadeResources(const Device& device){
    DescriptorBuilder desc_builder{};
    /* Buffer(s) */
    desc_builder.add_binding(0, vk::DescriptorType::eUniformBuffer);
    desc_builder.add_binding(1, vk::DescriptorType::eStorageBuffer);
    desc_builder.add_binding(2, vk::DescriptorType::eStorageBuffer);
    desc_builder.add_binding(3, vk::DescriptorType::eStorageBuffer);
    desc_builder.add_binding(4, vk::DescriptorType::eStorageBuffer);
    desc_builder.add_binding(5, vk::DescriptorType::eStorageBuffer);
    desc_builder.add_binding(6, vk::DescriptorType::eStorageImage);
    desc_builder.add_binding(7, vk::DescriptorType::eStorageImage);
    // desc_builder.add_binding(7, vk::DescriptorType::eCombinedImageSampler);

    /* Build the Surfel Cascade descriptor set */
    desc_set = desc_builder.build(device, vk::ShaderStageFlagBits::eCompute);

    /* Create a sampler for the radiance cache */
    vk::SamplerCreateInfo sampler_ci {};
    sampler_ci.setUnnormalizedCoordinates(true);
    sampler_ci.setMinFilter(vk::Filter::eLinear);
    sampler_ci.setMagFilter(vk::Filter::eLinear);
    sampler_ci.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
    sampler_ci.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
    sampler_ci.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
    const vk::ResultValue result = device.device.createSampler(sampler_ci);
    surfel_rad_sampler = result.value;
}

bool SurfelCascadeResources::alloc(const Device& device, const SurfelCascadeParameters& params, const uint32_t cascade_index) {
    /* Get Cascade parameters */
    this->cascade_index = cascade_index;
    const uint32_t surfel_cap = params.get_probe_capacity(cascade_index);
    const uint32_t grid_cap = params.get_grid_capacity(cascade_index);
    const uint32_t memory_width = params.get_memory_width(cascade_index);

    const buf::AllocParams alloc_ci{VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

    /* Allocate the Surfel parameters buffer */
    const uint32_t param_size = sizeof(SurfelCascadeParameters);
    if (!buf::alloc(device, surfel_param, {param_size, buf::Usage::eUniformBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;

    /* Allocate the Surfel stack */
    const uint32_t stack_size = sizeof(uint32_t) * (1u + surfel_cap);
    if (!buf::alloc(device, surfel_stack, {stack_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;

    /* Allocate the Surfel acceleration structure */
    const uint32_t hashgrid_size = sizeof(uint32_t) * (grid_cap + 1u); /* +1 because the last element has to be empty for prefix sum */
    if (!buf::alloc(device, surfel_grid, {hashgrid_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;
    const uint32_t hashlist_size = sizeof(uint32_t) * surfel_cap * 16u;
    if (!buf::alloc(device, surfel_list, {hashlist_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;

    /* Allocate the Surfel positions & radius */
    const uint32_t posr_size = sizeof(float) * 4u * surfel_cap;
    if (!buf::alloc(device, surfel_posr, {posr_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;

    /* Allocate the Surfel normals */
    const uint32_t norw_size = sizeof(float) * 4u * surfel_cap;
    if (!buf::alloc(device, surfel_norw, {norw_size, buf::Usage::eStorageBuffer | buf::Usage::eTransferDst}, alloc_ci)) return false;

    /* Allocate the Surfel Radiance texture */
    const uint32_t cache_width = memory_width * (uint32_t)sqrt(surfel_cap);
    if (img::Texture2D::make(device, surfel_rad, 
        {cache_width, cache_width}, 
        /* TODO: Find a proper way to store HDR radiance using 32 bits!!! */
        vk::Format::eR16G16B16A16Sfloat, // vk::Format::eR32G32B32A32Sfloat, // vk::Format::eA2B10G10R10UnormPack32, 
        vk::ImageAspectFlagBits::eColor, 
        vk::ImageLayout::eGeneral, 
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc
    ) == false) return false;
    if (img::Texture2D::make(device, surfel_merge, 
        {cache_width, cache_width}, 
        /* TODO: Find a proper way to store HDR radiance using 32 bits!!! */
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageAspectFlagBits::eColor, 
        vk::ImageLayout::eGeneral, 
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst
    ) == false) return false;

    /* Initialize the Surfel stack */
    uint32_t* init_stack = new uint32_t[1u + surfel_cap]{};
    init_stack[0] = 0u;
    for (uint32_t i = 0u; i < surfel_cap; ++i) init_stack[1u + i] = i;
    buf::upload(device, surfel_stack, init_stack, sizeof(uint32_t) * (1u + surfel_cap));
    delete[] init_stack;

    /* Initialize the Surfel parameters */
    buf::upload(device, surfel_param, &params, sizeof(SurfelCascadeParameters));

    /* Attach Surfel buffers */
    desc_set.attach_constant_buffer(device, 0, surfel_param.buffer, surfel_param.size);
    desc_set.attach_storage_buffer(device, 1, surfel_stack.buffer, surfel_stack.size);
    desc_set.attach_storage_buffer(device, 2, surfel_grid.buffer, surfel_grid.size);
    desc_set.attach_storage_buffer(device, 3, surfel_list.buffer, surfel_list.size);
    desc_set.attach_storage_buffer(device, 4, surfel_posr.buffer, surfel_posr.size);
    desc_set.attach_storage_buffer(device, 5, surfel_norw.buffer, surfel_norw.size);
    desc_set.attach_storage_image(device, 6, surfel_rad.view, device.nearest_sampler, vk::ImageLayout::eGeneral);
    desc_set.attach_storage_image(device, 7, surfel_merge.view, device.nearest_sampler, vk::ImageLayout::eGeneral);
    // desc_set.attach_image_sampler(device, 7, surfel_rad.view, surfel_rad_sampler, vk::ImageLayout::eGeneral);
    return true;
}

void SurfelCascadeResources::free_buffers(const Device& device) {
    /* Free the Surfel buffers */
    surfel_param.free(device);
    surfel_stack.free(device);
    surfel_grid.free(device);
    surfel_list.free(device);
    surfel_posr.free(device);
    surfel_norw.free(device);
    surfel_rad.free(device);
    surfel_merge.free(device);
}

void SurfelCascadeResources::free(const Device& device) {
    free_buffers(device);
    desc_set.free(device);
    device.device.destroySampler(surfel_rad_sampler);
}

bool SurfelCascadeResources::update_surfel_count(const Device& device, const SurfelCascadeParameters& params) {
    uint32_t val = 0u;
    const bool r = buf::extract_u32(device, surfel_stack, 0u, val);
    surfel_count = val;
    return r;
}

}  // namespace wyre
