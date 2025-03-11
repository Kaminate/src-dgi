#pragma once

#include "core/ecs.h" /* Entity */

namespace wyre {

enum class LogLevel;

class Device;
class Assets;
class ECS;
class Window;
class Input;
class Files;
class Logger;

/**
 * @brief Wyre engine instance.
 */
class WyreEngine final {
    /* Allow the renderer to access private variables */
    friend class Renderer;

    /* Graphics device (should remain private) */
    Device& device;

    Renderer* renderer = nullptr; /* Kept around for destruction */
    
   public:
    /* Core modules */
    Assets& assets;
    ECS& ecs;

    /* Active camera entity. (has to be set by the game!) */
    Entity active_camera { entt::null };

    /* System modules */
    Window& window;
    Input& input;
    Files& files;
    Logger& logger;

    WyreEngine() = delete;
    WyreEngine(const LogLevel&& log_level);
    ~WyreEngine();

    /**
     * @brief Initialize engine resources.
     * @return Boolean to indicate success or failure.
     */
    [[nodiscard]] bool init();

    /**
     * @brief Execute the engine main loop. (will block the thread)
     * @return Boolean to indicate failure in case of non-recoverable error.
     */
    [[nodiscard]] bool run();

    /**
     * @brief Free engine resources.
     * @return Boolean to indicate success or failure.
     */
    [[nodiscard]] bool destroy();
};

}  // namespace wyre
