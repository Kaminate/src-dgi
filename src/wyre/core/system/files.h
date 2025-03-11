#pragma once

#include <vector> /* std::vector */
#include <string> /* std::string */

#include "wyre/result.h" /* Result<T> */

namespace wyre {

class WyreEngine;

/**
 * @brief Central class for file loading/reading.
 */
class Files {
    /* Only let the engine construct a Files instance */
    friend class WyreEngine;
    Files() = default;
    ~Files() = default;

   public:
    /**
     * @brief Read the text contents of a file.
     */
    Result<std::string> read_text_file(const std::string& path) const;

    /**
     * @brief Read the binary contents of a file.
     */
    Result<std::vector<std::byte>> read_binary_file(const std::string& path) const;
};

}  // namespace wyre
