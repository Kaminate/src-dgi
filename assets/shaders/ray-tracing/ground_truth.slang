/**
 * @brief Compute kernel for debug drawing Surfels.
 * This kernel is purely for debugging purposes.
 */
import camera;
import tonemap;
import fast_math;
import bvh;
import hash;

/* Ground-truth descriptor set (0) */
[[vk::binding(0, 0)]] RWTexture2D<float4> accumulator; /* Radiance accumulator */

/* Attachments descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<renderview_t> renderview;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_albedo;
[[vk::binding(2, 1)]] RWTexture2D<float4> g_normal_depth;

/* Ray tracing descriptor set (2) */
[[vk::binding(0, 2)]] StructuredBuffer<basic_node> scene_bvh;
[[vk::binding(1, 2)]] StructuredBuffer<basic_tri> scene_prims;
[[vk::binding(2, 2)]] StructuredBuffer<basic_nor> scene_norms;

struct context_t {
    float alpha;
    uint frame_index;
}

/* Ground-truth push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/** @brief Get the current output resolution. */
inline uint2 get_resolution(const RWTexture2D<float4> tex) { uint2 r; tex.GetDimensions(r.x, r.y); return r; }

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
 
inline float3 random_unit_vector(inout uint state) {
    float z = random_float(state) * 2.0 - 1.0;
    float a = random_float(state) * 6.283185;
    float r = sqrt(1.0 - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

[shader("compute")] /* Compute shader entry point */
[numthreads(8, 8, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get the UV coordinate of the current pixel (0.0 -> 1.0) */
    const float2 frag_coord = thread_id.xy;
    const uint2 resolution = get_resolution(g_albedo);
    const float2 uv = frag_coord / resolution;

    /* Ignore emissive surfaces */
    if (g_albedo[thread_id.xy].w > 0.4 && g_albedo[thread_id.xy].w < 0.6) return;

    /* Find the world-space position of the current pixel */
    const float4 normal_depth = g_normal_depth[thread_id];
    const float3 pixel_normal = normalize(normal_depth.xyz);
    const float pixel_depth = normal_depth.w;
    if (pixel_depth > 1000.0) return;
    const float3 pixel_dir = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);
    const float3 pixel_pos = renderview.origin + pixel_dir * pixel_depth + pixel_normal * 0.0001;

    uint seed = uint(uint(thread_id.x) * uint(1973) + uint(thread_id.y) * uint(9277) + uint(context.frame_index) * uint(26699)) | uint(1);
    // const float3 rand_dir = normalize(random_unit_vector(seed) + pixel_normal);
    const float3 rand_dir = random_unit_vector(seed);

    /* Trace a random uniformly distributed ray */
    const hit_result hit = trace_bvh(
        pixel_pos, rand_dir, 1000.0,
        scene_bvh, scene_prims
    );

    /* If we hit, record any found radiance */
    float3 radiance = 0.0;
    if (hit.t < 1000.0) {
        const basic_tri tri = scene_prims[hit.prim_i];
        const float3 material = tri.get_color();
        const bool is_emissive = any(material > 0.0);

        if (is_emissive) {
            const float ndotl = saturate(dot(pixel_normal, rand_dir));
            radiance = material * RPI * ndotl;
        }
    }

    // const float ndotl = saturate(dot(pixel_normal, rand_dir));
    // const float3 radiance = 0.5 * RPI * ndotl;

    accumulator[thread_id].rgb = lerp(accumulator[thread_id].rgb, radiance, context.alpha);

    /* Apply albedo & normalize the weighted radiance sum to get final exitant radiance */
    const float3 exitant_radiance = g_albedo[thread_id.xy].rgb * accumulator[thread_id].rgb * PI4;

    g_albedo[thread_id.xy] = float4(aces(exitant_radiance), 1.0);
    // g_albedo[thread_id.xy] = float4(exitant_radiance, 1.0);
}
