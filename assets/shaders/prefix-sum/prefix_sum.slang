/**
 * @brief Compute kernel for running a prefix sum over the Surfel hash grid.
 * Source: <https://github.com/h3r2tic/kajiya/>
 */

/* Surfels descriptor set (0) */
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> surfel_grid; /* Surfel Hash Grid entry indices. */

#define THREAD_GROUP_SIZE 512
#define SEGMENT_SIZE (THREAD_GROUP_SIZE * 2)

/* Fetch 2 inputs from the Surfel Hash Grid */
inline uint2 load_input2(const uint idx, const uint segment) {
    return uint2(
        surfel_grid[idx + segment * SEGMENT_SIZE + 0u],
        surfel_grid[idx + segment * SEGMENT_SIZE + 1u]
    );
}

/* Store 2 outputs to the Surfel hash grid */
inline void store_output2(const uint idx, const uint segment, const uint2 val) {
    surfel_grid[idx + segment * SEGMENT_SIZE + 0u] = val.x;
    surfel_grid[idx + segment * SEGMENT_SIZE + 1u] = val.y;
}

groupshared uint gs_data[SEGMENT_SIZE];

[shader("compute")] /* Compute shader entry point */
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void entry_compute(uint idx: SV_GroupThreadID, uint segment: SV_GroupID) {
    static const uint STEP_COUNT = (uint)log2(THREAD_GROUP_SIZE) + 1u;

    /* Load data into the groupshared memory block */
    const uint2 input2 = load_input2(idx * 2u, segment);
    gs_data[idx * 2u + 0u] = input2.x;
    gs_data[idx * 2u + 1u] = input2.y;
    GroupMemoryBarrierWithGroupSync();

    [unroll] /* Perform a prefix sum on the groupshared memory block */
    for (uint step = 0; step < STEP_COUNT; ++step) {
        const uint mask = (1u << step) - 1;
        const uint rd_idx = ((idx >> step) << (step + 1)) + mask;
        const uint wr_idx = rd_idx + 1 + (idx & mask);

        gs_data[wr_idx] += gs_data[rd_idx];
        GroupMemoryBarrierWithGroupSync();
    }

    store_output2(idx * 2, segment, uint2(gs_data[idx * 2], gs_data[idx * 2 + 1]));
}
