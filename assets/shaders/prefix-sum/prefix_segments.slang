/**
 * @brief Compute kernel for running a prefix sum over the Surfel hash grid.
 * Source: <https://github.com/h3r2tic/kajiya/>
 */

/* Surfels descriptor set (0) */
[[vk::binding(2, 0)]] StructuredBuffer<uint> surfel_grid; /* Surfel Hash Grid entry indices. */

/* Prefix Segments descriptor set (1) */
[[vk::binding(0, 1)]] RWStructuredBuffer<uint> segments; /* Prefix Sum Segments. */

#define THREAD_GROUP_SIZE 512
#define SEGMENT_SIZE (THREAD_GROUP_SIZE * 2)

/* Fetch 2 inputs from the Surfel Hash Grid */
inline uint load_input(const uint idx) {
    const uint segment_sum_idx = idx * SEGMENT_SIZE + SEGMENT_SIZE - 1;
    return surfel_grid[segment_sum_idx];
}

/* Store 2 outputs to the Segments buffer */
inline void store_output2(const uint idx, const uint x, const uint y) {
    segments[idx + 0u] = x;
    segments[idx + 1u] = y;
}

groupshared uint gs_data[SEGMENT_SIZE];

[shader("compute")] /* Compute shader entry point */
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void entry_compute(uint idx: SV_GroupThreadID, uint segment: SV_GroupID) {
    const uint STEP_COUNT = (uint)log2(THREAD_GROUP_SIZE) + 1u;

    /* Load the last number of each segment into shared memory */
    gs_data[idx * 2u + 0u] = load_input(idx * 2u + 0u);
    gs_data[idx * 2u + 1u] = load_input(idx * 2u + 1u);
    GroupMemoryBarrierWithGroupSync();

    [unroll] /* Perform a prefix sum on the groupshared memory block */
    for (uint step = 0; step < STEP_COUNT; step++) {
        const uint mask = (1u << step) - 1u;
        const uint rd_idx = ((idx >> step) << (step + 1u)) + mask;
        const uint wr_idx = rd_idx + 1u + (idx & mask);

        gs_data[wr_idx] += gs_data[rd_idx];
        GroupMemoryBarrierWithGroupSync();
    }

    store_output2(idx * 2u, gs_data[idx * 2u + 0u], gs_data[idx * 2u + 1u]);
}
