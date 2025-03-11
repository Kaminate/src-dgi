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

inline float ground_t(const float3 ro, const float3 rd) {
    if (rd.y > 0.0) return 1e30;
    return ro.y / -rd.y;
}

static const float3 SUN_DIR = normalize(float3(1.0, 1.0, 1.0));
static const float3 SKY_COLOR = float3(0.4, 0.7, 1.0);
static const float3 SUN_COLOR = float3(1.0, 0.7, 0.1) * 10.0;
inline float3 get_sky_color(const float3 rd) {
    const float sundot = saturate(dot(rd, SUN_DIR));
    float3 col = SKY_COLOR * 1.1 - max(rd.y, 0.01) * max(rd.y, 0.01) * 0.5;
    col = lerp(col, 0.85 * SKY_COLOR, pow(1.0 - max(rd.y, 0.0), 6.0));

    col += 0.25 * SUN_COLOR * pow(sundot, 5.0);
    col += 0.25 * SUN_COLOR * pow(sundot, 64.0);
    col += 0.20 * SUN_COLOR * pow(sundot, 512.0);
    
    col += saturate((0.1 - rd.y) * 10.0) * float3(0.0, 0.1, 0.2);
    col += 0.2 * SUN_COLOR * pow(sundot, 8.0);
    return col * 0.1;
}

inline uint wang_hash(inout uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}
 
inline float random_float(inout uint state) {
    return float(wang_hash(state)) / 4294967296.0;
}

#define WHITE_FURNACE 0

/* Settings */
#define JITTER 0
#define JITTER_FRAMES 8
#define TEMPORAL 0
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
    const uint frame_idx = context.get_frame_index();

    /* Get the index of the Surfel we're gathering for */ 
    const uint2 surfel_id = thread_id / memory_width;
    const uint surfel_ptr = surfel_id.y * cache_size.x + surfel_id.x;
    const float4 posr = surfel_posr[surfel_ptr]; /* position & radius (packed) */

    /* Make sure this is a live Surfel */
    if (posr.w == 0.0) { 
        surfel_rad[thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        surfel_merge[thread_id] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    /* Get the index of the interval we're gathering */
    const uint2 interval_id = thread_id % memory_width;

#if TEMPORAL
    /* Only trace 1:16 rays each frame, to reduce ray tracing cost across frames */
    const uint2 trace_id = (thread_id / TEMPORAL_WIDTH) * TEMPORAL_WIDTH + uint2(frame_idx & TEMPORAL_MASK, (frame_idx >> TEMPORAL_SHFT) & TEMPORAL_MASK);
    if (any(thread_id != trace_id)) {
        surfel_merge[thread_id] = surfel_rad[thread_id];
        return;
    }
#endif

#if JITTER
    /* Randomly jitter intervals inside their solid angle */
    const uint frame_index = context.get_frame_index();
    uint seed = uint(uint(thread_id.x) * uint(1973) + uint(thread_id.y) * uint(9277) + uint(frame_index) * uint(26699)) | uint(1);
    const float2 jitter = float2(random_float(seed), random_float(seed));
    const float2 interval_uv = ((float2)interval_id + jitter) / memory_width;
#else
    const float2 interval_uv = ((float2)interval_id + 0.5) / memory_width;
#endif

    /* Get the direction of the interval we're gathering */
    const float3 interval_dir = oct_decode(interval_uv * 2.0 - 1.0);
    const float2 interval = params.get_interval(cascade_index);

    /* Applying the cosine term here, because doing it in the composite pass causes artifacts */
    /* However, applying it here also results in artifacts... */
    const float cos_theta = 1.0;
    // const float cos_theta = max(0.0, dot(surfel_norw[surfel_ptr].xyz, interval_dir));
    // if (cos_theta < 0.0) {
    //     surfel_rad[thread_id] = encode_interval(float4(0.0, 0.0, 0.0, 0.0));
    //     return;
    // }

#if WHITE_FURNACE
    /* White furnace test */
    if (cascade_index == 5u) surfel_rad[thread_id] = encode_interval(float4(0.5, 0.5, 0.5, 1.0));
    else surfel_rad[thread_id] = encode_interval(float4(0.0, 0.0, 0.0, 1.0));
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

#if JITTER
    surfel_rad[thread_id] = lerp(surfel_rad[thread_id], encode_interval(float4(radiance * cos_theta, visibility)), (1.0 / JITTER_FRAMES));
#else
    surfel_rad[thread_id] = encode_interval(float4(radiance * cos_theta, visibility));
#endif
    surfel_merge[thread_id] = surfel_rad[thread_id];
}
