/**
 * @file renderer.h
 * @brief Vulkan renderer.
 */
#pragma once

#include "api.h"

#include "wyre/core/ecs-system.h" /* wyre::System */

namespace wyre {

class Window;
class Logger;
class Device;

class GeometryStage;
class GIStage;
class FinalStage;

class SceneBvhMaintainer;
class SceneBvhPacker;

/**
 * @brief Vulkan renderer system.
 */
class Renderer : wyre::System {
    /* Maintainers */
    SceneBvhMaintainer& bvh_maintainer;
    SceneBvhPacker& bvh_packer;

    /* Stages */
    GeometryStage& geometry_stage;
    GIStage& gi_stage;
    FinalStage& final_stage;

    float last_dt = 1.0f;
    bool show_overlay = true;
    bool show_surfel = false;

   public:
    Renderer() = delete;
    explicit Renderer(Logger& logger, const wyre::Window& window, const wyre::Device& device);
    ~Renderer() override = default;

    /**
     * @brief Destroy any renderer resources. (should be called by the engine)
     */
    void destroy(const wyre::Device& device);

    void update(wyre::WyreEngine& engine, const float dt) override;

    void render(wyre::WyreEngine& engine) override;

    void overlay(wyre::WyreEngine& engine);
};

}  // namespace wyre
