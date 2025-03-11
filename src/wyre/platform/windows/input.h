#pragma once

#include <array> /* std::array */
#include <memory> /* std::unique_ptr */

#include <glm/glm.hpp>

#include "wyre/core/system/keycodes.h"

namespace wyre {

class Window;

/**
 * @brief Engine module for processing user input.
 */
class Input {
    friend class Window;

    /* Key state arrays */
    std::unique_ptr<std::array<bool, 512>> key_down = nullptr;
    std::unique_ptr<std::array<bool, 512>> key_up = nullptr;
    std::unique_ptr<std::array<bool, 512>> key_held = nullptr;

    /* Mouse button state arrays */
    std::unique_ptr<std::array<bool, 3>> mouse_down = nullptr;
    std::unique_ptr<std::array<bool, 3>> mouse_up = nullptr;
    std::unique_ptr<std::array<bool, 3>> mouse_held = nullptr;

    /* Mouse position */
    glm::ivec2 mouse_pos = {0, 0};

    /* Internal setters */
    void set_key_down(Key key);
    void set_key_up(Key key);
    void set_mouse_down(MouseButton button);
    void set_mouse_up(MouseButton button);

    /**
     * @brief Clear all the key & mouse button states.
     */
    void clear_state();

   public:
    Input();
    ~Input() = default;

    /**
     * @brief Get the current position of the mouse on screen. (in pixels)
     */
    const glm::ivec2& get_mouse_pos() const;

    /** @brief Is the given mouse button pressed this tick? */
    bool is_mouse_down(MouseButton button);
    /** @brief Is the given mouse button released this tick? */
    bool is_mouse_up(MouseButton button);
    /** @brief Is the given mouse button being held down this tick? */
    bool is_mouse_held(MouseButton button);

    /** @brief Is the given key pressed this tick? */
    bool is_key_down(Key key);
    /** @brief Is the given key released this tick? */
    bool is_key_up(Key key);
    /** @brief Is the given key being held down this tick? */
    bool is_key_held(Key key);
};

}  // namespace wyre