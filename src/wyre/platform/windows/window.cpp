#include "window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h> /* Windows always uses Vulkan for now. */
#include <imgui_impl_sdl3.h>

#include "wyre/core/system/input.h"

namespace wyre {

Window::~Window() {
    if (handle == nullptr) return;
    SDL_DestroyWindow(handle);
    SDL_Quit();
    handle = nullptr;
}

void Window::init(const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        open = false;
        return;
    }

    /* Create SDL window */
    handle = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN);
    open = true;
}

bool Window::init_imgui() const {
    return ImGui_ImplSDL3_InitForVulkan(handle);
}

void Window::poll_events(Input& input) {
    /* SDL Input handling */
    SDL_Event event = {};
    input.clear_state(); /* Clear all input state */

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            /* Window close event */
            case SDL_EVENT_QUIT: open = false; break;
            /* Keyboard input events */
            case SDL_EVENT_KEY_UP: input.set_key_up((Key)event.key.scancode); break;
            case SDL_EVENT_KEY_DOWN: input.set_key_down((Key)event.key.scancode); break;
            /* Mouse input events */
            case SDL_EVENT_MOUSE_BUTTON_UP: input.set_mouse_up((MouseButton)event.button.button); break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: input.set_mouse_down((MouseButton)event.button.button); break;
            case SDL_EVENT_MOUSE_MOTION: input.mouse_pos.x = event.motion.x, input.mouse_pos.y = event.motion.y; break;
            default: break;
        }

        ImGui_ImplSDL3_ProcessEvent(&event); /* ImGui input */
    }
}

bool Window::create_surface(const vk::Instance instance, vk::SurfaceKHR& out_surface) const {
    VkSurfaceKHR surface;
    const bool result = SDL_Vulkan_CreateSurface(handle, static_cast<VkInstance>(instance), nullptr, &surface);
    out_surface = vk::SurfaceKHR(surface);
    if (result == false) {
        printf("error: failed to create KHR surface!\nreason: %s\n", SDL_GetError());
    }
    return result;
}

}  // namespace wyre
