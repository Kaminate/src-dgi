/**
 * @brief Compute kernel for debug drawing Surfels.
 * This kernel is purely for debugging purposes.
 */
import camera;
import cascade;
import surfel;
import hash; /* pcg3d */

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> surfel_grid;   /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> surfel_list;   /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_albedo;
[[vk::binding(2, 1)]] RWTexture2D<float4> g_normal_depth;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/** @brief Get the current output resolution. */
inline uint2 get_resolution() { uint2 r; g_albedo.GetDimensions(r.x, r.y); return r; }

/** @brief Generate a random color based on a seed. */
inline float3 color(const uint seed) {
    const uint3 r = pcg3d(uint3(seed, seed << 12, seed << 24));
    return 0.25 + 0.75 * ((float3)r * (1.0 / float(0xffffffffu)));
}

[shader("compute")] /* Compute shader entry point */
[numthreads(16, 8, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get the UV coordinate of the current pixel (0.0 -> 1.0) */
    const float2 frag_coord = thread_id.xy;
    const uint2 resolution = get_resolution();
    const float2 uv = frag_coord / resolution;

    /* Find the world-space position of the current pixel */
    const float4 normal_depth = g_normal_depth[thread_id];
    const float3 pixel_normal = normal_depth.xyz;
    const float pixel_depth = normal_depth.w;
    if (pixel_depth > 1000.0f) return;
    const float3 pixel_dir = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);
    const float3 pixel_pos = renderview.origin + pixel_dir * pixel_depth;

    const uint cascade_index = context.get_cascade_index();
    const uint hashkey = surfel_cell_hash(pixel_pos, renderview.origin, params.get_grid_scale(cascade_index)) % params.get_grid_capacity(cascade_index);
    const uint start = surfel_grid[hashkey];
    const uint end = surfel_grid[hashkey + 1u];

    float4 c = 0.0;
    for (uint i = start; i < end; ++i) {
        /* Fetch the Surfel for coverage testing */
        const uint surfel_ptr = surfel_list[i];
        const float4 posr = surfel_posr[surfel_ptr];
        const float4 norw = surfel_norw[surfel_ptr];

        /* Surfel weight (for debug drawing only) */
        const float weight = point_coverage(pixel_pos, pixel_normal, posr, norw); // posr.w - sqr_dist(posr.xyz, pixel_pos);
        if (weight > 0.0) continue;

        /* Add the Surfel color based on its pointer */
        c += float4(color(surfel_ptr) * -weight, -weight);
        // const float heat = (float)surfel_count / params.cell_capacity;
        // c += float4(lerp(float3(0.2f, 0.2f, 1.0f), float3(1.0f, 0.2f, 0.2f), heat) * weight, weight);
    }

    /* Don't override color if no Surfels were found */
    if (c.a == 0.0) return;

    g_albedo[thread_id.xy] = float4(c.rgb * rcp(c.a), 1.0);
}
