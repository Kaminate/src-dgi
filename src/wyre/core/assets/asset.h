#pragma once

namespace wyre {

/**
 * @brief Asset base class.
 */
struct Asset {
    size_t id = 0;

    virtual ~Asset() = default;
};

}  // namespace wyre
