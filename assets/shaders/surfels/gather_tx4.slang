/**
 * @brief Compute kernel for gathering Surfel radiance from the scene.
 * Tracing intervals through the scene using octahedral mapping.
 */
import surfel;
import cascade;
import bvh;
import octahedral; /* oct_xxx */
import sky;

/* Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> params;     /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] StructuredBuffer<uint> surfel_stack;  /* [0] = stack pointer. */
[[vk::binding(2, 0)]] StructuredBuffer<uint> surfel_grid;   /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> surfel_list;   /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] StructuredBuffer<float4> surfel_posr; /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] StructuredBuffer<float4> surfel_norw; /* xyz = normal, w = recycle marker */
[[vk::binding(6, 0)]] RWTexture2D<float4> surfel_rad;       /* Surfel radiance cache */
[[vk::binding(7, 0)]] RWTexture2D<float4> surfel_merge;     /* Surfel merged radiance cache */

/* Ray tracing descriptor set (1) */
[[vk::binding(0, 1)]] StructuredBuffer<basic_node> scene_bvh;
[[vk::binding(1, 1)]] StructuredBuffer<basic_tri> scene_prims;
[[vk::binding(2, 1)]] StructuredBuffer<basic_nor> scene_norms;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/** @brief Get the current output resolution. */
inline uint2 get_resolution() { uint2 r; surfel_rad.GetDimensions(r.x, r.y); return r; }

#define WHITE_FURNACE 0

/* Settings */
#define JITTER 0
#define JITTER_FRAMES 4
#define TEMPORAL 1
#define TEMPORAL_WIDTH 4
#define TEMPORAL_MASK (TEMPORAL_WIDTH - 1)
#define TEMPORAL_SHFT ((uint)log2(TEMPORAL_WIDTH))

[shader("compute")] /* Compute shader entry point */
[numthreads(16, 16, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get cascade properties */
    const uint cascade_index = context.get_cascade_index();
    const uint memory_width = params.get_memory_width(cascade_index);
    const uint interval_count = memory_width * memory_width;
    const uint2 cache_size = get_resolution() / memory_width;

    /* This kernel will be run for 1:16 pixels */
    const uint frame_idx = context.get_frame_index();
    const uint2 pixel_id = thread_id * 4u + uint2(frame_idx & 3u, (frame_idx >> 2u) & 3u);

    /* Get the index of the Surfel we're gathering for */ 
    const uint2 surfel_id = pixel_id / memory_width;
    const uint surfel_ptr = surfel_id.y * cache_size.x + surfel_id.x;
    const float4 posr = surfel_posr[surfel_ptr]; /* position & radius (packed) */

    /* Make sure this is a live Surfel */
    if (posr.w == 0.0) { 
        surfel_rad[pixel_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    /* Get the index of the interval we're gathering */
    const uint2 interval_id = pixel_id % memory_width;
    const float2 interval_uv = ((float2)interval_id + 0.5) / memory_width;

    /* Get the direction of the interval we're gathering */
    const float3 interval_dir = oct_decode(interval_uv * 2.0 - 1.0);
    const float2 interval = params.get_interval(cascade_index);

    /* Applying the cosine term here, because doing it in the composite pass causes artifacts */
    /* However, applying it here also results in artifacts... */
    const float cos_theta = max(0.0, dot(surfel_norw[surfel_ptr].xyz, interval_dir));
    if (cos_theta < 0.0) {
        surfel_rad[pixel_id] = encode_interval(float4(0.0, 0.0, 0.0, 0.0));
        return;
    }

#if WHITE_FURNACE
    /* White furnace test */
    if (cascade_index == 5u) surfel_rad[pixel_id] = encode_interval(float4(0.5, 0.5, 0.5, 1.0));
    else surfel_rad[pixel_id] = encode_interval(float4(0.0, 0.0, 0.0, 1.0));
    return;
#endif

    /* Trace the interval */
    const float3 ro = posr.xyz + interval_dir * interval.x;
    const float3 rd = interval_dir;
    const float tmax = interval.y - interval.x;
    const hit_result hit = trace_bvh(
        ro, rd, tmax,
        scene_bvh, scene_prims
    );
    // const float ground = ground_t(ro, rd);
    // const float t = min(hit.t, ground > tmax ? 1e30 : ground);

    float3 radiance = 0.0;
    float visibility = (hit.t >= tmax) ? 1.0 : 0.0;
    if (visibility == 1.0) {
        if (cascade_index == 5u) radiance = getSkyColor(rd, getAnimatedSunDir((float)frame_idx * 0.01666)); // float3(0.4, 0.7, 1.0) * 4.0;
    } else {
        const basic_tri tri = scene_prims[hit.prim_i];
        const float3 material = tri.get_color();
        const bool is_emissive = any(material > 0.0);
        if (is_emissive) radiance = material;
    }

    surfel_rad[pixel_id] = encode_interval(float4(radiance * cos_theta, visibility));
}
