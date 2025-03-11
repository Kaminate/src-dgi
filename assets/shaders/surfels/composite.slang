/**
 * @brief Compute kernel for debug drawing Surfels.
 * This kernel is purely for debugging purposes.
 */
import camera;
import cascade;
import surfel;
import octahedral; /* oct_xxx */
import tonemap;
import fast_math;
import hash;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] StructuredBuffer<uint> surfel_stack;    /* [0] = stack pointer. */
[[vk::binding(2, 0)]] StructuredBuffer<uint> surfel_grid;     /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> surfel_list;     /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] StructuredBuffer<float4> surfel_posr;   /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */
[[vk::binding(6, 0)]] RWTexture2D<float4> surfel_rad;         /* Surfel radiance cache */
[[vk::binding(7, 0)]] RWTexture2D<float4> surfel_merge;       /* Surfel merged radiance cache */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_albedo;
[[vk::binding(2, 1)]] RWTexture2D<float4> g_normal_depth;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/** @brief Get the current output resolution. */
inline uint2 get_resolution(const RWTexture2D<float4> tex) { uint2 r; tex.GetDimensions(r.x, r.y); return r; }

/* Swap the contents of 2 variables. */
inline void swap<T>(inout T a, inout T b) { const T c = a; a = b, b = c; }

#define COS_SUM 0

[shader("compute")] /* Compute shader entry point */
[numthreads(8, 8, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get the UV coordinate of the current pixel (0.0 -> 1.0) */
    const float2 frag_coord = thread_id.xy;
    const uint2 resolution = get_resolution(g_normal_depth);
    const float2 uv = frag_coord / resolution;

    if (g_albedo[thread_id.xy].w > 0.4 && g_albedo[thread_id.xy].w < 0.6) return;

    /* Find the world-space position of the current pixel */
    const float3 pixel_normal = normalize(g_normal_depth[thread_id].xyz);
    const float pixel_depth = g_normal_depth[thread_id].w;
    if (pixel_depth <= 0.0 || pixel_depth >= 1000.0) return;
    const float3 pixel_dir = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);
    const float3 pixel_pos = renderview.origin + pixel_dir * pixel_depth + (pixel_normal * 0.00001);

    /* Get the Cascade parameters */
    const uint cascade_index = context.get_cascade_index();
    const uint grid_capacity = params.get_grid_capacity(cascade_index);
    const float grid_scale = params.get_grid_scale(cascade_index);

    /* Fetch nearby Surfels from the source Cascade using the hash grid */
    const uint hashkey = surfel_cell_hash(pixel_pos, renderview.origin, grid_scale) % grid_capacity;
    const uint start = surfel_grid[hashkey];
    const uint end = surfel_grid[hashkey + 1u];

    float4 dists = 8.0; /* Find the 4 best interpolation candidates */
    uint4 src_ptrs = 0xffffffff;
    for (uint i = start; i < end; ++i) {
        /* Fetch the next Surfel pointer from the list */
        const uint surfel_ptr = surfel_list[i];
        if (any(surfel_ptr == src_ptrs)) continue; /* Ignore duplicate Surfels */

        /* Fetch the position & normal of the Surfel */
        const float3 surfel_pos = surfel_posr[surfel_ptr].xyz;
        const float3 surfel_nor = surfel_norw[surfel_ptr].xyz;

        /* Calculate the distance and normal difference of the Surfel */
        const float surfel_dist = distance2(pixel_pos, surfel_pos);
        const float surfel_diff = (1.0 - dot(pixel_normal, surfel_nor));

        float dist = surfel_dist + surfel_diff * surfel_diff;
        if (dist > dists.w) continue; /* Early skip */
        uint ptr = surfel_ptr;
        [unroll] for (uint j = 0; j < 4u; ++j) {
            if (dist < dists[j]) {
                swap(dists[j], dist);
                swap(src_ptrs[j], ptr);
            }
        }
    }

    /* Generate normalized weights based on Surfel distance */
    const float4 weights = 1.0 / (dists + 0.0001);
    const float weights_sum = weights.x + weights.y + weights.z + weights.w;

    const uint memory_width = params.get_memory_width(cascade_index);
    const float inv_memory_width = 1.0 / memory_width;
    const float solid_contribution = inv_memory_width * inv_memory_width;
    const uint2 cache_size = get_resolution(surfel_rad) / memory_width;

    /* Integrate outgoing radiance based on nearby Surfels */
    float3 radiance_sum = 0.0;
    for (uint i = 0u; i < 4u; ++i) {
        if (src_ptrs[i] == 0xffffffff) continue;

        /* Find the location of the Surfel radiance in the cache */
        const uint surfel_ptr = src_ptrs[i];
        const uint2 surfel_id = uint2(surfel_ptr % cache_size.x, surfel_ptr / cache_size.x);
        const uint2 cache_id = surfel_id * memory_width;
        surfel_norw[surfel_ptr].w = 1.0;

        /* Integrate outgoing radiance for Surfel */
        float3 surfel_radiance = 0.0;
        for (uint v = 0u; v < memory_width; ++v) {
            for (uint u = 0u; u < memory_width; ++u) {
                const uint2 interval_id = uint2(u, v);

                // float cos_theta = 0.0;
                // for (float y = 0.25; y < 1.25; y += 0.5) {
                //     for (float x = 0.25; x < 1.25; x += 0.5) {
                //         const float2 interval_uv = ((float2)interval_id + float2(x, y)) * inv_memory_width;
                //         const float3 interval_dir = oct_decode(interval_uv * 2.0 - 1.0);
                //         cos_theta += max(0.0, dot(pixel_normal, interval_dir)) * 0.25;
                //     }
                // }

                const float2 interval_uv = ((float2)interval_id + 0.5) * inv_memory_width;
                const float3 interval_dir = oct_decode(interval_uv * 2.0 - 1.0);
                const float cos_theta = max(0.0, dot(pixel_normal, interval_dir));
                
                // const float3 h = normalize(-pixel_dir + interval_dir);
                // const float ndoth = pow(saturate(dot(pixel_normal, h)), 16.0);

                /* TODO: I can already apply Lambert's cosine law during the merge! */
                /* Because I already know the surface normal, which is the Surfel normal. */

                surfel_radiance += decode_interval(surfel_merge[cache_id + interval_id]).rgb * RPI * cos_theta;
            }
        }

        radiance_sum += surfel_radiance * solid_contribution * weights[i];
    }

    /* Apply albedo & normalize the weighted radiance sum to get final exitant radiance */
    const float3 exitant_radiance = g_albedo[thread_id.xy].rgb * (radiance_sum / weights_sum) * PI4;

    /* TODO: Tonemapping should be a seperate pass. */
    g_albedo[thread_id.xy] = float4(aces(exitant_radiance), 1.0);
    // g_albedo[thread_id.xy] = float4(exitant_radiance, 1.0);
}
