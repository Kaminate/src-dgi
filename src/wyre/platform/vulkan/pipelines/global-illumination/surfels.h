#pragma once

#include <cstdint>

namespace wyre {

/* Maximum number of Surfel per acceleration structure grid cell. */
constexpr uint32_t SAS_CELL_CAPACITY = 12u - 1u;

/** @brief Surfel Acceleration Structure Cell. */
struct SASCell {
    /* Number of Surfels in this cell. */
    uint32_t surfel_count = 0u;
    /* List of Surfels in this cell. */
    uint32_t surfels[SAS_CELL_CAPACITY]{};
};

}  // namespace wyre
