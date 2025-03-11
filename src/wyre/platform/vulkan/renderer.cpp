/**
 * @file renderer.cpp
 * @brief Vulkan renderer.
 */
#include "renderer.h"

#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>

#include "stages/geometry.h"            /* GeometryStage */
#include "stages/global-illumination.h" /* GIStage */
#include "stages/final.h"               /* FinalStage */

#include "wyre/core/scene/bvh-maintainer.h" /* SceneBvhMaintainer */
#include "vulkan/scene/bvh-packer.h"        /* SceneBvhPacker */

#include "wyre/core/graphics/device.h"
#include "wyre/core/components/transform.h"
#include "wyre/core/components/camera.h"
#include "wyre/core/system/input.h"
#include "wyre/wyre.h"

namespace wyre {

/* Initialize the renderer stages */
Renderer::Renderer(Logger& logger, const wyre::Window& window, const wyre::Device& device)
    : bvh_maintainer(*new SceneBvhMaintainer()), 
      bvh_packer(*new SceneBvhPacker(logger, device)),
      geometry_stage(*new GeometryStage(logger, device, bvh_packer.bvh_desc)),
      gi_stage(*new GIStage(logger, window, device, bvh_packer.bvh_desc)),
      final_stage(*new FinalStage(logger, window, device)) {}

void Renderer::destroy(const wyre::Device& device) {
    /* Destroy stages */
    geometry_stage.destroy(device);
    delete &geometry_stage;
    gi_stage.destroy(device);
    delete &gi_stage;
    final_stage.destroy(device);
    delete &final_stage;

    /* Destroy maintainers */
    delete &bvh_maintainer;
    bvh_packer.destroy(device);
    delete &bvh_packer;
}

void Renderer::update(wyre::WyreEngine& engine, const float dt) { 
    last_dt = dt; 
    if (engine.input.is_key_down(Key::KEY_GRAVE)) {
        show_overlay = !show_overlay;
    }
}

void Renderer::render(wyre::WyreEngine& engine) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = engine.device.get_frame().gcb;
    const buf::Buffer& render_view = engine.device.get_frame().render_view;
    
    /* New ImGui frame */
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    /* Get the render view data */
    const Entity& camera_entity = engine.active_camera;
    const Transform* transform = engine.ecs.try_get_component<Transform>(camera_entity);
    const Camera* camera = engine.ecs.try_get_component<Camera>(camera_entity);
    if (!transform || !camera) return;
    const glm::vec3 forward = glm::vec4(0, 0, 1, 0) * transform->rotation;
    const glm::mat4 view = glm::lookAt(transform->position, transform->position + forward, {0, 1, 0});
    const glm::mat4 inv_view = glm::inverse(view);
    const glm::mat4 proj = camera->get_projection(engine.window, 0.1f, 1000.0f);

    /* Update the render view buffer */
    const RenderView current_view = RenderView {
        .view = view, 
        .proj = proj, 
        .inv_view = inv_view,
        .inv_proj = glm::inverse(proj),
        .origin = transform->position, 
        .fov = glm::radians(camera->fov)
    };
    buf::upload(engine.device, render_view, &current_view, sizeof(RenderView));

    /* Maintain */
    bvh_maintainer.maintain(engine.ecs);

    /* Package the BVH and send it to the GPU */
    bvh_packer.package(engine.device, bvh_maintainer.bvh);
    const DescriptorSet& bvh = bvh_packer.bvh_desc;

    overlay(engine); /* <- debug overlay */

    /* Queue the render stages in order */
    geometry_stage.enqueue(engine.window, engine.device, bvh);
    
    gi_stage.enqueue(engine.window, engine.device, bvh);
    final_stage.enqueue(engine.window, engine.device);
}

void Renderer::overlay(wyre::WyreEngine& engine) {
    if (show_overlay == false) return;

    const ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoTitleBar;
    constexpr float PADDING = 10.0f;
    ImVec2 work_pos = ImGui::GetMainViewport()->WorkPos;
    ImVec2 work_size = ImGui::GetMainViewport()->WorkSize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    /* Performance Overlay */
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + PADDING, work_pos.y + PADDING), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize({0, 0});

    ImGui::Begin("Performance", nullptr, overlay_flags);
    ImGui::Text("FPS: %f", 1.0f / last_dt);
    ImGui::End();
    
    /* Surfel Overlay */
    constexpr int SURFEL_OVERLAY_WIDTH = 180;
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + work_size.x - SURFEL_OVERLAY_WIDTH - PADDING, work_pos.y + PADDING), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize({SURFEL_OVERLAY_WIDTH, 0});

    ImGui::Begin("Surfels", nullptr, overlay_flags);
    ImGui::Text("Surfel Usage: ");
    ImGui::SameLine();
    ImGui::Text("(%u)", gi_stage.cascades[gi_stage.debug_cascade_index].surfel_count);
    ImGui::ProgressBar((float)gi_stage.cascades[gi_stage.debug_cascade_index].surfel_count / gi_stage.cascade_params.get_probe_capacity(gi_stage.debug_cascade_index));
    if (ImGui::Button(show_surfel ? "Close Debugger" : "Open Debugger")) {
        show_surfel = !show_surfel;
    }
    ImGui::End();

    /* Surfel Debugger */
    if (show_surfel) {
        ImGui::Begin("Surfel Debugger");

        static uint32_t cascade_count = 7u;
        uint32_t interval_count = gi_stage.cascade_params.c0_memory_width * gi_stage.cascade_params.c0_memory_width;

        ImGui::BeginTabBar("Surfel Debugger Tabs");

        if (ImGui::BeginTabItem("Cascade Parameters")) {
            ImGui::SeparatorText("Parameters");

            ImGui::Checkbox("Draw Surfels", &gi_stage.direct_draw);
            ImGui::SameLine();
            ImGui::Dummy({8.0f, 0.0f});
            ImGui::SameLine();
            ImGui::Checkbox("Surfel Heatmap", &gi_stage.heatmap);
            ImGui::SameLine();
            ImGui::Dummy({8.0f, 0.0f});
            ImGui::SameLine();
            ImGui::Checkbox("Ground Truth", &gi_stage.ground_truth);

            ImGui::BeginTable("Surfel Cascade Parameters", 2);
            
            ImGui::TableNextColumn();
            ImGui::Text("[db] cascade index");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##cascade_index", ImGuiDataType_U32, &gi_stage.debug_cascade_index);
            if (gi_stage.debug_cascade_index >= CASCADE_COUNT) gi_stage.debug_cascade_index = CASCADE_COUNT - 1u;
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] grid capacity");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##grid_capacity", ImGuiDataType_U32, &gi_stage.cascade_params.c0_grid_capacity);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] grid scale");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##grid_scale", ImGuiDataType_Float, &gi_stage.cascade_params.c0_grid_scale);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[cN] cell capacity");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##cell_capacity", ImGuiDataType_U32, &gi_stage.cascade_params.cell_capacity);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] memory width");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##memory_width", ImGuiDataType_U32, &gi_stage.cascade_params.c0_memory_width);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] interval count");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##interval_count", ImGuiDataType_U32, &interval_count, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] probe capacity");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##probe_capacity", ImGuiDataType_U32, &gi_stage.cascade_params.c0_probe_capacity, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[c0] probe radius");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##probe_radius", ImGuiDataType_Float, &gi_stage.cascade_params.c0_probe_radius);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[cN] maximum solid angle");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##max_solid_angle", ImGuiDataType_Float, &gi_stage.cascade_params.max_solid_angle);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("[cN] cascade count");
            ImGui::TableNextColumn();
            ImGui::InputScalar("##cascade_count", ImGuiDataType_U32, &cascade_count);
            ImGui::TableNextRow();

            ImGui::EndTable();

            if (ImGui::Button("Push Parameters")) {
                gi_stage.update_params(engine.logger, engine.device);
            }
            
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Cascade Statistics")) {
            ImGui::SeparatorText("Statistics");

            ImGui::BeginTable("Cascades", 4, ImGuiTableFlags_ScrollX, {0, 152});

            ImGui::TableNextColumn();
            ImGui::Text("Cascade");
            ImGui::TableNextColumn();
            ImGui::Text("Probe Capacity");
            ImGui::TableNextColumn();
            ImGui::Text("Probe Intervals");
            ImGui::TableNextColumn();
            ImGui::Text("Interval Length");
            ImGui::TableNextRow();
            
            constexpr int SPATIAL_FACTOR = 4;
            constexpr int ANGULAR_FACTOR = 4;

            const float base_interval = gi_stage.cascade_params.max_solid_angle * (float)interval_count / 12.56637f / ANGULAR_FACTOR;

            /* Cascades */
            uint32_t total_rays = 0u;
            uint32_t total_surfels = 0u;
            for(int i = 0; i < cascade_count; ++i) {
                ImGui::TableNextColumn();
                ImGui::Text("[c%i]", i);
                const uint32_t probes = gi_stage.cascade_params.c0_probe_capacity / pow(SPATIAL_FACTOR, i);
                const uint32_t intervals = interval_count * pow(ANGULAR_FACTOR, i);
                float interval_start = base_interval * pow(ANGULAR_FACTOR, i);
                float interval_end = base_interval * pow(ANGULAR_FACTOR, i + 1);
                if (i == 0) {   
                    interval_start = 0.0f;
                    interval_end = base_interval * ANGULAR_FACTOR;
                }
                
                ImGui::TableNextColumn();
                ImGui::Text("%u", probes);
                ImGui::TableNextColumn();
                ImGui::Text("%u", intervals);
                ImGui::TableNextColumn();
                ImGui::Text("(%.2f, %.2f] +%.2f", interval_start, interval_end, interval_end - interval_start);

                total_rays += probes * intervals;
                total_surfels += probes;
                ImGui::TableNextRow();
            }

            ImGui::EndTable();

            /* Ray count */
            ImGui::Text("Total Rays: %.2f GRays (%u)", (float)total_rays / 1'000'000'000.0f, total_rays);
            ImGui::Text("60FPS Rays: %.2f GRays/s", (float)total_rays / 1'000'000'000.0f * 60.0f);

            /* Memory */
            // const uint32_t radiance_mem = total_rays * sizeof(uint32_t) /* rgb9e5 */;
            // ImGui::Text("Radiance Memory: %.2f MB (rgb9e5)", radiance_mem / 1'000'000.0f);
            // const uint32_t surfel_mem = gi_stage.cascade.surfel_posr.size 
            //     + gi_stage.cascade.surfel_norw.size
            //     + gi_stage.cascade.surfel_stack.size
            //     + gi_stage.cascade.surfel_accel.size;
            // ImGui::Text("Surfel Memory: %.2f MB", surfel_mem / 1'000'000.0f);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        ImGui::End();
    }

    ImGui::PopStyleVar();
}

}  // namespace wyre
