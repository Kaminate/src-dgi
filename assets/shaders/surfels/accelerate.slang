/**
 * @brief Compute kernel for inserting Surfels into the acceleration structure.
 * This is re-done at the start of each frame, because the acceleration structure moves.
 */
import surfel;
import cascade;
import camera;
import atomic; /* atomic_xxx */
import fast_math; /* fast_xxx */
import hash;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;     /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] StructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> surfel_grid; /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> surfel_list; /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] StructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] StructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/* Check if a Surfel overlaps a grid cell. */
// inline bool surfel_to_cell(const GridContext ctx, const float3 pos, const float radius, const int3 cell) {
//     /* Find the world-space center of the grid cell */
//     const float3 cell_center = ((float3)(cell - ctx.grid_size / 2) + 0.5) * ctx.units_per_cell; // + renderview.origin;
//     const float seperation_radius = radius + ctx.units_per_cell * 0.9; /* 2.0 -> sqrt(3.0) = 0.866 ~> 0.9 */

//     /* Check if the grid cell & Surfel are seperated (approximate) */
//     return distance2(cell_center, pos) < (seperation_radius * seperation_radius);
// }

/* Insert a Surfel into the Surfel Hash Grid for a given grid level. */
inline void insert_surfel_level(const int3 min, const int3 max, const uint surfel_ptr, const uint level, const uint grid_capacity, const uint list_bounds) {
    /* Insert the Surfel into every grid location it covers */
    for (int z = min.z; z <= max.z; ++z) {
        for (int y = min.y; y <= max.y; ++y) {
            for (int x = min.x; x <= max.x; ++x) {
                /* Hash the location */
                const uint hashkey = surfel_hash_function(asuint(int3(x, y, z)), level) % grid_capacity;

                /* Decrement the atomic counter in the Surfel Hash Cell */
                const uint offset = atomic_dec(&surfel_grid[hashkey]) - 1u;
                if (offset >= list_bounds) return;
                atomic_set(&surfel_list[offset], surfel_ptr);
                // surfel_list[offset] = surfel_ptr;
            } 
        } 
    }   
}

/* Insert a Surfel into the Surfel Hash Grid. */
inline void insert_surfel(const float3 pos, const float radius, const uint surfel_ptr, const uint cascade_index) {
    const uint grid_capacity = params.get_grid_capacity(cascade_index);
    const float grid_scale = params.get_grid_scale(cascade_index);
    const uint list_bounds = params.get_list_bounds(cascade_index);

    /* Find the min/max Surfel grid level */
    const uint2 grid_level = surfel_grid_level_minmax(pos, radius, renderview.origin, grid_scale);
    const float3 min_pos = pos - radius;
    const float3 max_pos = pos + radius;

    /* Always insert into the first level */
    const float min_grid_size = surfel_grid_size(grid_level.x, grid_scale);
    insert_surfel_level(
        surfel_grid_position(min_pos, min_grid_size),
        surfel_grid_position(max_pos, min_grid_size),
        surfel_ptr, grid_level.x, grid_capacity, list_bounds
    );

    if (grid_level.x == grid_level.y) return; /* Early out */

    /* Also insert into the second level if it's different */
    const float max_grid_size = surfel_grid_size(grid_level.y, grid_scale);
    insert_surfel_level(
        surfel_grid_position(min_pos, max_grid_size),
        surfel_grid_position(max_pos, max_grid_size),
        surfel_ptr, grid_level.y, grid_capacity, list_bounds
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

    /* Insert Surfel into its Hash Cell */
    insert_surfel(posr.xyz, sqrt(posr.w), surfel_ptr, cascade_index);
}
