/**
 * @file pipelines/scene-bvh.h
 * @brief Scene BVH Maintainer.
 */
#pragma once

#include "./bvh.h"

namespace wyre {

class Logger;
class ECS;

/**
 * @brief Scene BVH Maintainer, keeps the scene BVH updated.
 */
class SceneBvhMaintainer {
    friend class Renderer;

    scene::Bvh bvh {};

    SceneBvhMaintainer() = default;
    ~SceneBvhMaintainer() = default;

    /**
     * @brief Maintain the scene BVH.
     */
    void maintain(ECS& ecs);
};

}  // namespace wyre
