/**
 * @brief Compute kernel for spawning Surfels from the GBuffers.
 * Spawned Surfels are immediately inserted into the acceleration structure.
 */
import surfel;
import cascade;
import camera;
import hash; /* pcg1d */
import atomic; /* atomic_xxx */
import fast_math;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] StructuredBuffer<uint> surfel_grid;     /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> surfel_list;     /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;
[[vk::binding(1, 1)]] Texture2D<float4> g_albedo;
[[vk::binding(2, 1)]] Texture2D<float4> g_normal_depth;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

static const uint SPAWN_CHANCE = 0xffffffff;

/** @brief Get the current output resolution. */
inline uint2 get_resolution() { uint2 r; g_albedo.GetDimensions(r.x, r.y); return r; }

#define SCALAR 0

/* Best Surfel spawn candidate in the group.
   Packing: `MS16b` Coverage, `LS16b` Local XY */
groupshared uint gs_candidate;

[shader("compute")] /* Compute shader entry point */
[numthreads(8, 8, 1)]
void entry_compute(
    uint2 thread_id : SV_DispatchThreadID, 
    uint  local_idx : SV_GroupIndex
) {
    /* This kernel will be run for 1:4 pixels */
    const uint frame_idx = context.get_frame_index();
    const uint2 pixel_id = thread_id * 2u + uint2(frame_idx & 1u, (frame_idx >> 1u) & 1u);

    if (g_albedo[pixel_id].w > 0.4 && g_albedo[pixel_id].w < 0.6) return;

    /* Reset the group shared Surfel candidate */
    gs_candidate = 0x00000000u;

    /* Get the UV coordinate of the current pixel (0.0 -> 1.0) */
    const uint2 resolution = get_resolution();
    const float2 uv = (float2)pixel_id / resolution;

    /* Find the world-space position of the current pixel */
    const float3 pixel_normal = normalize(g_normal_depth[pixel_id].xyz);
    const float pixel_depth = g_normal_depth[pixel_id].w;
    if (pixel_depth <= 0.0 || pixel_depth >= 1000.0) return; /* <- Important to avoid NaN Surfels */
    const float3 pixel_dir = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);

    /* Don't spawn anything at grazing angles to avoid floating point imprecision */
    if (dot(-pixel_dir, pixel_normal) < 0.01) return;

    /* Location in world-space that this pixel is seeing */
    const float3 pixel_pos = renderview.origin + pixel_dir * pixel_depth + (pixel_normal * 0.0001);

    /* Find the current pixel location in the grid structure */
    const uint cascade_index = context.get_cascade_index();
    const uint grid_capacity = params.get_grid_capacity(cascade_index);
    const float grid_scale = params.get_grid_scale(cascade_index);
    
    /* Check coverage around the location, to make sure coverage isn't too dense */
    const uint v_hashkey = surfel_cell_hash(pixel_pos, renderview.origin, grid_scale) % grid_capacity;
    const uint v_start = surfel_grid[v_hashkey];
    const uint v_end = surfel_grid[v_hashkey + 1u];

#if SCALAR
    /* Scalarized Surfel coverage threshold testing */
    float v_coverage = 1e30;
    for (uint v_offset = v_start; v_offset < v_end;) {
        const uint s_offset = WaveActiveMin(v_offset);

        /* If this lane matches the lane min, process this Surfel */
        if (s_offset == v_offset) {
            v_offset++; /* Increment our lane's offset */
            const uint s_surfel_ptr = surfel_list[s_offset];
            const float4 s_posr = surfel_posr[s_surfel_ptr];
            const float4 s_norw = surfel_norw[s_surfel_ptr];
            v_coverage = min(v_coverage, point_coverage(pixel_pos, pixel_normal, s_posr, s_norw));
            if (v_coverage < 1.0) return; /* Early out */
        }
    }
#else
    float v_coverage = 1e30;
    for (uint i = v_start; i < v_end; ++i) {
        /* Fetch the Surfel for coverage testing */
        const uint surfel_ptr = surfel_list[i];
        const float4 posr = surfel_posr[surfel_ptr];
        const float4 norw = surfel_norw[surfel_ptr];
        const float cover = point_coverage(pixel_pos, pixel_normal, posr, norw);
        if (cover < 1.0) return;
        v_coverage = max(v_coverage, cover);
    }
#endif

    /* Coverage as fixed-point uint */
    const uint score = min(65535u, (uint)(v_coverage * 1000.0));
    const uint candidate = ((score << 16u) & 0xffff0000) | (local_idx & 0x0000ffff);

    /* Find the pixel with the lowest coverage in this group */
    atomic_max(&gs_candidate, candidate);
    GroupMemoryBarrierWithGroupSync();

    /* Only one lane continues */
    if (gs_candidate != candidate) return;
    
    const uint seed = context.get_frame_index() * resolution.x * resolution.y + pixel_id.y * resolution.x + pixel_id.x;
    if (pcg1d(seed) > (SPAWN_CHANCE / pow(2u, cascade_index + 0u))) return; /* Random chance */

    /* Push the Surfel stack (atomic) */
    const uint stack_ptr = atomic_inc(&surfel_stack[0]);
    const uint surfel_cap = params.get_probe_capacity(cascade_index);
    if (stack_ptr >= surfel_cap) { 
        atomic_set(&surfel_stack[0], surfel_cap);
        return; /* Surfel stack is full */
    }
    const uint surfel_ptr = surfel_stack[1u + stack_ptr];

    /* Set the attributes of the new Surfel */
    const float perspective_correct = params.get_probe_radius(cascade_index) * pixel_depth * renderview.fov;
    const float radius = max(params.get_probe_min_radius(cascade_index), perspective_correct);
    surfel_posr[surfel_ptr] = float4(pixel_pos, radius * radius);
    surfel_norw[surfel_ptr] = float4(pixel_normal, 2.0);
}
