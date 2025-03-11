#pragma once

#include <entt/entity/registry.hpp> /* entt::registry */

#include "ecs-system.h" /* wyre::System */
#include "wyre/result.h" /* Result<T> */

namespace wyre {

class WyreEngine;

/**
 * @brief ECS entity instance.
 */
using Entity = entt::entity;

/**
 * @brief Manager for Entities, Components, & Systems.
 */
class ECS {
    /* Only let the Engine construct an ECS */
    friend wyre::WyreEngine;
    ECS();
    ~ECS();

    /* Non-copyable */
    ECS(const ECS&) = delete; 
    ECS& operator=(const ECS&) = delete; 

    /* Internal functions */
    void systems_update(WyreEngine& engine, const float dt);
    void systems_render(WyreEngine& engine);
    void remove_deleted();

    /* Deletion tag component for entities to be deleted */
    struct Delete {};

    std::vector<std::unique_ptr<System>> systems = {};

   public:
    /* EnTT Registry */
    entt::registry registry = {};
    
    /**
     * @brief Create an entity instance.
     * 
     * @return The entity instance.
     */
    Entity create_entity() { return { registry.create() }; }

    /**
     * @brief Destroy an entity instance.
     */
    void destroy_entity(Entity entity);
    
    /**
     * @brief Add a component to an entity instance.
     * 
     * @param entity Entity instance to add the component to.
     * @param args Arguments to be forwarded to the component constructor.
     * 
     * @return The component instance.
     */
    template <typename T, typename... Args>
    decltype(auto) add_component(Entity entity, Args&&... args) {
        return registry.emplace<T>(entity, std::forward<Args>(args)...);
    }

    /**
     * @brief Get a component of type attached to an entity.
     */
    template <typename T>
    T& get_component(const Entity entity) {
        return registry.get<T>(entity);
    }

    /**
     * @brief Get a component of type attached to an entity.
     */
    template <typename T>
    T* try_get_component(const Entity entity) {
        return registry.try_get<T>(entity);
    }
    
    /**
     * @brief Register a new system to the ECS.
     * 
     * @param args Arguments to be forwarded to the system constructor.
     * 
     * @return A reference to the system instance.
     */
    template <typename T, typename... Args>
    T& register_system(Args&&... args) {
        /* Create the system instance & store it in the systems vector */
        T* system = new T(std::forward<Args>(args)...);
        systems.push_back(std::unique_ptr<System>(reinterpret_cast<System*>(system)));
        return *system;
    }
};

}  // namespace wyre
