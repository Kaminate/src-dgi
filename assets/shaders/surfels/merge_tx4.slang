/**
 * @brief Compute kernel for merging Surfel cascades into each other.
 * This is called from top to bottom of the cascade hierarchy.
 */
import surfel;
import cascade;
import camera;
import fast_math;
import hash;

/* [CascadeN] Surfels descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<cascade_t> dst_params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 0)]] StructuredBuffer<uint> dst_surfel_stack;    /* [0] = stack pointer. */
[[vk::binding(2, 0)]] StructuredBuffer<uint> dst_surfel_grid;     /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 0)]] StructuredBuffer<uint> dst_surfel_list;     /* Surfel Hash Grid entries list. */
[[vk::binding(4, 0)]] StructuredBuffer<float4> dst_surfel_posr;   /* xyz = position, w = radius squared */
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> dst_surfel_norw; /* xyz = normal, w = recycle marker */
[[vk::binding(6, 0)]] RWTexture2D<float4> dst_surfel_rad;         /* Surfel radiance cache */
[[vk::binding(7, 0)]] RWTexture2D<float4> dst_surfel_merge;       /* Surfel merged radiance cache */

/* [CascadeN+1] Surfels descriptor set (1) */
[[vk::binding(0, 1)]] ConstantBuffer<cascade_t> src_params;       /* Surfel Cascade parameters. */
[[vk::binding(1, 1)]] StructuredBuffer<uint> src_surfel_stack;    /* [0] = stack pointer. */
[[vk::binding(2, 1)]] StructuredBuffer<uint> src_surfel_grid;     /* Surfel Hash Grid entry indices. */
[[vk::binding(3, 1)]] StructuredBuffer<uint> src_surfel_list;     /* Surfel Hash Grid entries list. */
[[vk::binding(4, 1)]] StructuredBuffer<float4> src_surfel_posr;   /* xyz = position, w = radius squared */
[[vk::binding(5, 1)]] RWStructuredBuffer<float4> src_surfel_norw; /* xyz = normal, w = recycle marker */
[[vk::binding(6, 1)]] RWTexture2D<float4> src_surfel_rad;         /* Surfel radiance cache */
[[vk::binding(7, 1)]] RWTexture2D<float4> src_surfel_merge;       /* Surfel merged radiance cache */
// [[vk::binding(7, 1)]] Sampler2D<float4> src_surfel_rad_ro;     /* Surfel radiance cache */

/* Attachments descriptor set (2) */
[[vk::binding(0, 2)]] ConstantBuffer<renderview_t> renderview;

/* Surfel Cascade context push constants */ 
[[vk::push_constant]] ConstantBuffer<context_t> context;

/* Get the resolution of a texture. */
inline uint2 get_resolution(Texture2D<float4> tex) { uint2 r; tex.GetDimensions(r.x, r.y); return r; }
inline uint2 get_resolution(RWTexture2D<float4> tex) { uint2 r; tex.GetDimensions(r.x, r.y); return r; }

/* Convert 4 1D indices to 4 2D indices in a 2x2 pattern. */
inline uint2 cvt_4x1_2x2(const uint offset_index) {
    const uint2 offsets[4] = { uint2(0u, 0u), uint2(1u, 0u), uint2(0u, 1u), uint2(1u, 1u) };
    return offsets[offset_index];
}

/* Swap the contents of 2 variables. */
inline void swap<T>(inout T a, inout T b) { const T c = a; a = b, b = c; }

[shader("compute")] /* Compute shader entry point */
[numthreads(16, 16, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get cascade properties */
    const uint dst_cascade_index = context.get_cascade_index();
    const uint dst_memory_width = dst_params.get_memory_width(dst_cascade_index);
    const uint2 dst_cache_size = get_resolution(dst_surfel_rad) / dst_memory_width;

    /* Fetch the destination Surfel */
    const uint2 dst_surfel_id = thread_id / dst_memory_width;
    const uint dst_surfel_ptr = dst_surfel_id.y * dst_cache_size.x + dst_surfel_id.x;
    const float4 dst_posr = dst_surfel_posr[dst_surfel_ptr]; /* position & radius (packed) */

    /* Make sure this is a live Surfel */
    if (dst_posr.w == 0.0) return;
    const float4 dst_norw = dst_surfel_norw[dst_surfel_ptr];
    
    /* Get the source Cascade parameters */
    const uint src_cascade_index = context.get_cascade_index() + 1u;
    const uint src_grid_capacity = src_params.get_grid_capacity(src_cascade_index);
    const float src_grid_scale = src_params.get_grid_scale(src_cascade_index);

    /* Fetch nearby Surfels from the source Cascade using the hash grid */
    const uint hashkey = surfel_cell_hash(dst_posr.xyz, renderview.origin, src_grid_scale) % src_grid_capacity;
    const uint start = src_surfel_grid[hashkey];
    const uint end = src_surfel_grid[hashkey + 1u];

    float4 dists = 512.0; /* Find the 4 best interpolation candidates */
    uint4 src_ptrs = 0xffffffff;
    for (uint i = start; i < end; ++i) {
        /* Fetch the next Surfel pointer from the list */
        const uint surfel_ptr = src_surfel_list[i];
        if (any(surfel_ptr == src_ptrs)) continue; /* Ignore duplicate Surfels */

        /* Fetch the position & normal of the Surfel */
        const float4 surfel_posr = src_surfel_posr[surfel_ptr];
        const float4 surfel_norw = src_surfel_norw[surfel_ptr];

        /* Calculate the distance and normal difference of the Surfel */
        const float surfel_dist = distance(dst_posr.xyz, surfel_posr.xyz);
        const float surfel_diff = (1.0 - dot(dst_norw.xyz, surfel_norw.xyz));

        // if (surfel_dist > surfel_posr.w * 4.0) continue;

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

    // if (any(src_ptrs == 0xffffffff)) {
    //     dst_surfel_rad[thread_id] = float4(10.0, 0.0, 0.0, 1.0);
    //     return;
    // }

    /* Generate normalized weights based on Surfel distance */
    const float4 weights = 1.0 / (dists + 0.0001);
    const float weights_sum = weights.x + weights.y + weights.z + weights.w;

    /* Find the ID of the destination interval & fetch its radiance */
    const uint2 dst_interval_id = thread_id % dst_memory_width;
    const bool reset_dst_surfel = dst_surfel_norw[dst_surfel_ptr].w > 1.0;
    if (reset_dst_surfel) { 
        dst_surfel_rad[thread_id] = encode_interval(float4(0.0, 0.0, 0.0, 0.0));
        dst_surfel_norw[dst_surfel_ptr].w = 1.0;
    }
    const float4 near_radiance = decode_interval(dst_surfel_rad[thread_id]);

    const uint src_memory_width = src_params.get_memory_width(src_cascade_index);
    const uint2 src_cache_size = get_resolution(src_surfel_rad) / src_memory_width;

    /* Interpolate with the source cascade Surfels */
    float4 radiance_sum = float4(0.0, 0.0, 0.0, 0.0);
    for (uint i = 0u; i < 4u; ++i) {
        if (src_ptrs[i] == 0xffffffff) continue;

        /* Find the location of the source radiance data in the source radiance cache */
        const uint src_surfel_ptr = src_ptrs[i];
        const uint2 src_surfel_id = uint2(src_surfel_ptr % src_cache_size.x, src_surfel_ptr / src_cache_size.x);
        const uint2 src_cache_id = src_surfel_id * src_memory_width;
        const uint2 src_interval_id = dst_interval_id * 2u; /* <- angular branch factor 4x */
        src_surfel_norw[src_surfel_ptr].w = 1.0;

        /* Merge 4 far radiance intervals into 1 near radiance interval */
        float4 far_radiance = float4(0.0, 0.0, 0.0, 0.0);
        for (uint j = 0u; j < 4u; ++j) {
            const uint2 offset_id = cvt_4x1_2x2(j);
            const uint2 sample_id = src_cache_id + src_interval_id + offset_id;
        
            // const uint2 src_pixel_id = (sample_id / 2u) * 2u + frame_id;

            /* Fetch the source radiance interval to merge with */
            if (src_cascade_index == 5u) {
                far_radiance += merge_intervals(near_radiance, decode_interval(src_surfel_rad[sample_id])) * 0.25;
            } else {
                far_radiance += merge_intervals(near_radiance, decode_interval(src_surfel_merge[sample_id])) * 0.25;
            }
        }

        /* Sample the average radiance from 4 intervals using bilinear sampler */
        // const float4 far_radiance = decode_interval(src_surfel_rad_ro.SampleLevel(src_cache_id + src_interval_id + 1u, 0.0));

        /* Merge far into near with respect to the visibility term */
        radiance_sum += far_radiance * weights[i];
    }

    /* Save the merged radiance after normalizing it */
    dst_surfel_merge[thread_id] = encode_interval(radiance_sum / weights_sum);
}
