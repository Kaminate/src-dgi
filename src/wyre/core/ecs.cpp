#include "ecs.h"

// #include "transform.h" /* Transform */

namespace wyre {

constexpr float kMaxDeltaTime = 1.0f / 30.0f;

ECS::ECS() = default;
ECS::~ECS() = default;

void ECS::destroy_entity(Entity entity) {
    /* Just return if the entity isn't valid */
    if (registry.valid(entity) == false) return;

    /* Tag entity for deletion */
    registry.emplace_or_replace<Delete>(entity);

    /* TODO: Transform child/parent logic for deletion goes here. */
    // auto* transform = registry.try_get<Transform>(entity.id);
    // if (transform != nullptr) {
    //     // detach from the parent entity
    //     transform->SetParent(entt::null);
    //     // recursively mark child entities as well
    //     for (auto child : *transform) DeleteEntity(child);
    // }
}

void ECS::systems_update(WyreEngine& engine, const float dt) {
    /* Clamp delta time to avoid spikes */
    const float safe_dt = std::min(dt, kMaxDeltaTime);
    for (auto& s : systems) { 
        /* Execute both versions of update */
        s->update(safe_dt);
        s->update(engine, safe_dt);
    }
}

void ECS::systems_render(WyreEngine& engine) {
    for (auto& s : systems) { 
        /* Execute both versions of render */
        s->render();
        s->render(engine);
    }
}

void ECS::remove_deleted() {
    /* Deleting entities can cause other entities to be deleted. */
    /* That's why we need to do the deletion in a loop. */
    bool queue_empty = false;
    while (queue_empty == false) {
        const auto del = registry.view<Delete>();
        registry.destroy(del.begin(), del.end());
        queue_empty = del.empty();
    }
}

}  // namespace wyre
