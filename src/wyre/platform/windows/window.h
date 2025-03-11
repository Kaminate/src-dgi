#pragma once

#include <vulkan/vulkan.hpp> /* Windows always uses Vulkan for now. */

struct SDL_Window;

namespace wyre {

class Input;

/**
 * @brief Engine module for windowing.
 */
class Window {
    friend class WyreEngine;
    friend class Device;
    friend class OverlayPipeline; /* init_imgui */

    SDL_Window* handle = nullptr;

    Window() = default;
    ~Window();

    void init(const char* title);

    /**
     * @brief Initialize ImGui backend.
     * (SDL3 in the case of Windows)
     */
    bool init_imgui() const;

    /**
     * @brief Poll window events, including all user input.
     * 
     * @param input Engine input module.
     */
    void poll_events(Input& input);

    /* NOTE: For now Windows will always use Vulkan, in the future, */
    /* We can implement multiple versions of `create_surface` for other gapi's. */
    bool create_surface(const vk::Instance instance, vk::SurfaceKHR& out_surface) const;

   public:
    uint32_t width = 1920, height = 1080;
    bool open = false;
};

}
