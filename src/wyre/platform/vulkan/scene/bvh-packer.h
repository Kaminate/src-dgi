#pragma once

#include "vulkan/api.h"

#include "vulkan/hardware/descriptor.h" /* DescriptorSet */
#include "vulkan/hardware/buffer.h"     /* buf:: */

namespace wyre {

namespace scene {
struct Bvh;
}

class Logger;
class Device;

/**
 * @brief Vulkan bvh packer. (sends BVH to the GPU)
 */
class SceneBvhPacker {
    friend class Renderer;
    
    buf::Buffer bvh_nodes{}; /* AABB Nodes */
    buf::Buffer bvh_prims{}; /* Vertices */
    buf::Buffer bvh_norms{}; /* Normals */

    SceneBvhPacker() = delete;
    explicit SceneBvhPacker(Logger& logger, const Device& device);
    ~SceneBvhPacker() = default;

    /**
     * @brief Destroy any resources. (should be called by the engine)
     */
    void destroy(const Device& device);

    /**
     * @brief Package the given BVH.
     */
    void package(const Device& device, const scene::Bvh& bvh);

   public:
    /* Descriptor set */
    DescriptorSet bvh_desc{};
};

}  // namespace wyre
