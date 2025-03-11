#include "input.h"

namespace wyre {

Input::Input() {
    key_down = std::make_unique<std::array<bool, 512>>();
    key_up = std::make_unique<std::array<bool, 512>>();
    key_held = std::make_unique<std::array<bool, 512>>();

    key_down->fill(false), key_up->fill(false), key_held->fill(false);

    mouse_down = std::make_unique<std::array<bool, 3>>();
    mouse_up = std::make_unique<std::array<bool, 3>>();
    mouse_held = std::make_unique<std::array<bool, 3>>();

    mouse_down->fill(false), mouse_up->fill(false), mouse_held->fill(false);
}

void Input::set_key_down(Key key) {
    if (key_held->at((size_t)key) == true) return;

    key_down->at((size_t)key) = true;
    key_held->at((size_t)key) = true;
}

void Input::set_key_up(Key key) {
    key_up->at((size_t)key) = true;
    key_held->at((size_t)key) = false;
}

void Input::set_mouse_down(MouseButton button) {
    if (button >= 3) return;
    button = (MouseButton)(button - 1);
    if (mouse_held->at((size_t)button) == true) return;

    mouse_down->at((size_t)button) = true;
    mouse_held->at((size_t)button) = true;
}

void Input::set_mouse_up(MouseButton button) {
    if (button >= 3) return;
    button = (MouseButton)(button - 1);
    mouse_up->at((size_t)button) = true;
    mouse_held->at((size_t)button) = false;
}

void Input::clear_state() {
    key_down->fill(false);
    key_up->fill(false);
    mouse_down->fill(false);
    mouse_up->fill(false);
}

const glm::ivec2& Input::get_mouse_pos() const { return mouse_pos; }

bool Input::is_key_down(Key key) { return key_down->at((size_t)key); }
bool Input::is_key_up(Key key) { return key_up->at((size_t)key); }
bool Input::is_key_held(Key key) { return key_held->at((size_t)key); }

bool Input::is_mouse_down(MouseButton button) { return mouse_down->at((size_t)button); }
bool Input::is_mouse_up(MouseButton button) { return mouse_up->at((size_t)button); }
bool Input::is_mouse_held(MouseButton button) { return mouse_held->at((size_t)button); }

}  // namespace wyre
