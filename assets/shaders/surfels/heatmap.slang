/**
 * @brief Compute kernel for debug drawing Surfel heatmap.
 * This kernel is purely for debugging purposes.
 */
import camera;
import cascade;
import surfel;
import hash;

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

/** @brief Generate a color based on how full a cell is. */
inline float3 color(const float fill) {
    const float level = fill * 1.57079632;
    return float3(sin(level), sin(level * 2.0), cos(level));
}

/* Settings */
#define COLOR_SCALE (1.0 / 16.0)

[shader("compute")] /* Compute shader entry point */
[numthreads(16, 16, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get the UV coordinate of the current pixel (0.0 -> 1.0) */
    const float2 frag_coord = thread_id.xy;
    const uint2 resolution = get_resolution();
    const float2 uv = frag_coord / resolution;

    /* Find the world-space position of the current pixel */
    const float4 normal_depth = g_normal_depth[thread_id];
    const float pixel_depth = normal_depth.w;
    if (pixel_depth > 1000.0) return;
    const float3 pixel_dir = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);
    const float3 pixel_pos = renderview.origin + pixel_dir * pixel_depth;

    /* Find the current pixel location in the grid structure */
    const uint cascade_index = context.get_cascade_index();
    const uint grid_capacity = params.get_grid_capacity(cascade_index);
    const float grid_scale = params.get_grid_scale(cascade_index);

    /* Fetch the grid cell and render its Surfel count as a color */
    const uint hashkey = surfel_cell_hash(pixel_pos, renderview.origin, grid_scale) % grid_capacity;
    const uint count = surfel_grid[hashkey + 1] - surfel_grid[hashkey];
    g_albedo[thread_id.xy] = float4(color(saturate((float)count * COLOR_SCALE)), 1.0);
}
