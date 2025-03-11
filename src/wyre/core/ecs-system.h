#pragma once

namespace wyre {

class WyreEngine;

/**
 * @brief ECS System, used to query entities & transform their components.
 * 
 * @warning Don't forget to register new systems with the ECS.
 */
class System {
    int priority = -1;

   public:
    virtual ~System() = default;

    /**
     * @brief Called once per game tick.
     */
    virtual void update(const float dt) { /* ... */ }
    
    /**
     * @brief Called once per game tick.
     */
    virtual void update(WyreEngine& engine, const float dt) { /* ... */ }

    /**
     * @brief Called once per frame during rendering.
     */
    virtual void render() { /* ... */ }
    
    /**
     * @brief Called once per frame during rendering.
     */
    virtual void render(WyreEngine& engine) { /* ... */ }
};

}
