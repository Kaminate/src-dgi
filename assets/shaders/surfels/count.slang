/**
 * @brief Compute kernel for counting Surfels per Hash Grid Cell.
 * This is used to determine the number of Surfels per Hash Grid Cell.
 */
import surfel;
import cascade;
import camera;
import atomic; /* atomic_xxx */
import hash;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;     /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] StructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> surfel_grid; /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> surfel_list;   /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] StructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] StructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/* Increment a counter in the Hash Grid Cell for a given grid level which the Surfel falls into. */
inline void count_surfel_level(const int3 min, const int3 max, const uint level, const uint grid_capacity) {
    /* Count the Surfel into every grid location it covers */
    for (int z = min.z; z <= max.z; ++z) {
        for (int y = min.y; y <= max.y; ++y) {
            for (int x = min.x; x <= max.x; ++x) {
                /* Hash the location */
                const uint hashkey = surfel_hash_function(asuint(int3(x, y, z)), level) % grid_capacity;

                /* Increment the atomic counter in the Surfel Hash Cell */
                atomic_inc(&surfel_grid[hashkey]);
            } 
        } 
    }   
}

/* Increment a counter in the Hash Grid Cell which the Surfel falls into. */
inline void count_surfel(const float3 pos, const float radius, const uint cascade_index) {
    const uint grid_capacity = params.get_grid_capacity(cascade_index);
    const float grid_scale = params.get_grid_scale(cascade_index);

    /* Find the min/max Surfel grid level */
    const uint2 grid_level = surfel_grid_level_minmax(pos, radius, renderview.origin, grid_scale);
    const float3 min_pos = pos - radius;
    const float3 max_pos = pos + radius;

    /* Always count into the first level */
    const float min_grid_size = surfel_grid_size(grid_level.x, grid_scale);
    count_surfel_level(
        surfel_grid_position(min_pos, min_grid_size),
        surfel_grid_position(max_pos, min_grid_size),
        grid_level.x, grid_capacity
    );

    if (grid_level.x == grid_level.y) return; /* Early out */

    /* Also count into the second level if it's different */
    const float max_grid_size = surfel_grid_size(grid_level.y, grid_scale);
    count_surfel_level(
        surfel_grid_position(min_pos, max_grid_size),
        surfel_grid_position(max_pos, max_grid_size),
        grid_level.y, grid_capacity
    );
}

[shader("compute")] /* Compute shader entry point */
[numthreads(128, 1, 1)]
void entry_compute(uint thread_id : SV_DispatchThreadID) {
    /* Fetch the Surfel we're working with */
    const uint surfel_ptr = thread_id;
    const float4 posr = surfel_posr[surfel_ptr]; /* position & radius (packed) */

    /* Make sure this is a live Surfel */
    if (posr.w == 0.0) return;

    /* Get the parameters from the current Cascade */
    const uint cascade_index = context.get_cascade_index();

    /* Count Surfel into its Hash Cell */
    count_surfel(posr.xyz, sqrt(posr.w), cascade_index);
}
