/**
 * @brief Compute kernel for running a prefix sum over the Surfel hash grid.
 * Source: <https://github.com/h3r2tic/kajiya/>
 */

/* Surfels descriptor set (0) */
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> surfel_grid; /* Surfel Hash Grid entry indices. */

/* Prefix Segments descriptor set (1) */
[[vk::binding(0, 1)]] StructuredBuffer<uint> segments; /* Prefix Sum Segments. */

#define THREAD_GROUP_SIZE 512
#define SEGMENT_SIZE (THREAD_GROUP_SIZE * 2)

/* Fetch 2 inputs from the Surfel Hash Grid */
inline uint2 load_input2(const uint idx, const uint segment) {
    const uint x = surfel_grid[idx + segment * SEGMENT_SIZE + 0u];
    const uint y = surfel_grid[idx + segment * SEGMENT_SIZE + 1u];
    const uint prev_segment_sum = segment == 0u ? 0u : segments[segment - 1u];

    return uint2(x, y) + prev_segment_sum;
}

/* Store 2 outputs to the Surfel Hash Grid */
void store_output2(uint idx, uint segment, uint2 val) {
    surfel_grid[idx + segment * SEGMENT_SIZE + 0u] = val.x;
    surfel_grid[idx + segment * SEGMENT_SIZE + 1u] = val.y;
}

groupshared uint gs_data[SEGMENT_SIZE];

[shader("compute")] /* Compute shader entry point */
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void entry_compute(uint idx: SV_GroupThreadID, uint segment: SV_GroupID) {
    store_output2(idx * 2u, segment, load_input2(idx * 2u, segment));
}
