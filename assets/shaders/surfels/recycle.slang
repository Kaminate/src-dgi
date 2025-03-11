/**
 * @brief Compute kernel for recycling Surfels.
 * This called at the end of each frame.
 */
import surfel;
import cascade;
import camera;
import atomic; /* atomic_xxx */
import hash;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] StructuredBuffer<uint> surfel_grid;     /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> surfel_list;     /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

#define COVERAGE_RECYCLING 1
#define FRUSTUM_RECYCLING 1

/* Allowed Frustum Error */
static const float FRUSTUM_ERROR = 0.5;

/* Any coverage above the threshold will get a chance to be recycled */
static const float COVERAGE_THRESHOLD = 0.25;
/* How quickly the chance of being recycled increases with coverage */
static const float COVERAGE_MULT = 1.0;

/* Check if a point lies within the view frustum. */
inline bool in_frustum(const float4x4 vp, const float3 p, const float e) {
    const float4 clip = mul(vp, float4(p, 1.0)) + float4(0.0, 0.0, e, e);
    return abs(clip.x) < clip.w && abs(clip.y) < clip.w && 0 < clip.z && clip.z < clip.w;
}

/* Atomically push a Surfel back onto the Surfel stack. */
inline void stack_push(const uint id) {
    /* Push Surfel back onto stack (atomic) */
    surfel_posr[id].w = 0.0;
    const uint slot = atomic_dec(&surfel_stack[0]) - 1u;
    atomic_set(&surfel_stack[1u + slot], id);
}

inline float random_float(const uint seed) {
    return float(pcg1d(seed)) / 4294967296.0;
}

/* Evaluate the recycling heuristic *(0.0 -> 1.0)* for a given Surfel. */
inline float heuristic(const uint surfel_ptr, const float4 posr, const float4 norw) {
    float heu = 0.0;

    /* [100%] If the recycling marker is not 0.0 */
    if (norw.w < 0.0) return 1.0;

    /* Check coverage around the Surfel, to make sure coverage isn't too dense */
    const uint cascade_index = context.get_cascade_index();
    const uint hashkey = surfel_cell_hash(posr.xyz, renderview.origin, params.get_grid_scale(cascade_index)) % params.get_grid_capacity(cascade_index);
    const uint start = surfel_grid[hashkey];
    const uint end = surfel_grid[hashkey + 1u];

    float coverage = 0.0;
    for (uint i = start; i < end; ++i) {
        /* Fetch the Surfel for coverage testing */
        const uint other_ptr = surfel_list[i];
        if (other_ptr == surfel_ptr) continue;
        const float4 other_posr = surfel_posr[other_ptr];
        const float4 other_norw = surfel_norw[other_ptr];

        /* Find the highest surfel coverage */
        const float surfel_coverage = point_coverage(other_posr.xyz, other_norw.xyz, posr, norw);
        if (surfel_coverage < 0.5) {
            heu += 0.2;
        }
        // coverage = max(coverage, surfel_coverage);
    }
    // heu += saturate((coverage - COVERAGE_THRESHOLD) * COVERAGE_MULT);

    return heu;
} 

inline bool is_nan(const float x) { return (asuint(x) & 0x7fffffffu) > 0x7f800000u; }

[shader("compute")] /* Compute shader entry point */
[numthreads(256, 1, 1)]
void entry_compute(uint thread_id : SV_DispatchThreadID) {
    /* Fetch the Surfel we're working with */
    const uint surfel_ptr = thread_id;

    const float4 posr = surfel_posr[surfel_ptr]; /* position & radius (packed) */
    if (posr.w == 0.0) return; /* Early out if this Surfel is not alive */
    const float4 norw = surfel_norw[surfel_ptr]; /* normal & recycle marker (packed) */

    /* Update the radius of the Surfels */
    const uint cascade_index = context.get_cascade_index();
    const float surfel_dist = max(1.0, distance(posr.xyz, renderview.origin));
    const float radius = params.get_probe_radius(cascade_index) * surfel_dist * renderview.fov;
    surfel_posr[surfel_ptr].w = radius * radius;
    surfel_norw[surfel_ptr].w -= 0.125;

    /* Evaluate the recycling heuristic */
    const float heu = heuristic(surfel_ptr, posr, norw);

    /* Randomly recycle based on the heuristic */
    const uint seed = context.get_frame_index() + surfel_ptr;
    if (random_float(seed) >= heu) return;
    stack_push(surfel_ptr);

    /* Check if this Surfel has been marked for recycling */
    // if (norw.w != 0.0) { stack_push(surfel_ptr); return; }

    /* Update radius of each Surfel */
    // const uint cascade_index = context.get_cascade_index();
    // const float surfel_dist = max(1.0, distance(posr.xyz, renderview.origin));
    // const float radius = params.get_probe_radius(cascade_index) * surfel_dist * renderview.fov;
    // surfel_posr[surfel_ptr].w = radius * radius;

// #if FRUSTUM_RECYCLING
//     /* Check if the Surfel is still inside the view frustum */
//     if (in_frustum(mul(renderview.proj, renderview.view), posr.xyz, FRUSTUM_ERROR) == false) { stack_push(surfel_ptr); return; }
// #endif

// #if COVERAGE_RECYCLING
//     /* Check coverage around the Surfel, to make sure coverage isn't too dense */
//     const uint hashkey = surfel_cell_hash(posr.xyz, renderview.origin, params.get_grid_scale(cascade_index)) % params.get_grid_capacity(cascade_index);
//     const uint start = surfel_grid[hashkey];
//     const uint end = surfel_grid[hashkey + 1u];

//     bool recycle = false;
//     for (uint i = start; i < end; ++i) {
//         /* Fetch the Surfel for coverage testing */
//         const uint other_ptr = surfel_list[i];
//         if (other_ptr == surfel_ptr) continue;
//         const float4 other_posr = surfel_posr[other_ptr];
//         const float4 other_norw = surfel_norw[other_ptr];

//         /* Recycle if Surfels are overlapping */
//         const float coverage = point_coverage(other_posr.xyz, other_norw.xyz, posr, norw); // posr.w - sqr_dist(other_posr.xyz, posr.xyz);
//         if (coverage < 0.5) recycle = true; // { stack_push(surfel_ptr); return; }
//     }

//     if (recycle) {
//         const uint seed = context.get_frame_index() + surfel_ptr;
//         if (pcg1d(seed) > (0xffffffffu / 2u)) return; /* Random chance */
//         stack_push(surfel_ptr);
//     }
// #endif
}
