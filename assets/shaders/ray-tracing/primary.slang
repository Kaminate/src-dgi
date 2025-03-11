import camera;
import bvh;
import tonemap;
import sky;

/* Attachments descriptor set (0) */
[[vk::binding(0, 0)]] ConstantBuffer<renderview_t> renderview;
[[vk::binding(1, 0)]] RWTexture2D<float4> out_albedo;
[[vk::binding(2, 0)]] RWTexture2D<float4> out_normal_depth;

/* Ray tracing descriptor set (1) */
[[vk::binding(0, 1)]] StructuredBuffer<basic_node> scene_bvh;
[[vk::binding(1, 1)]] StructuredBuffer<basic_tri> scene_prims;
[[vk::binding(2, 1)]] StructuredBuffer<basic_nor> scene_norms;

[[vk::push_constant]] ConstantBuffer<uint> frame_idx;

float2 ground_hit(float3 ro, float3 rd) {
    if (rd.y > 0.0) return 1e30;
    return ro.xz + rd.xz * (ro.y / -rd.y);
}

float checkers(float2 hit) {
    float2 p = floor(hit);
    return fmod(p.x + p.y, 2.0);
}

[shader("compute")] /* Compute shader entry point */
[numthreads(16, 8, 1)]
void entry_compute(uint2 thread_id : SV_DispatchThreadID) {
    /* Get the current pixel & the resolution of the output */
    const float2 frag_coord = thread_id;
    uint2 resolution;
    out_albedo.GetDimensions(resolution.x, resolution.y);

    /* UV coordinate of the current pixel, 0.0 -> 1.0 */
    const float2 uv = frag_coord / resolution;

    /* Get the ray traveling through the current pixel */
    const float3 ro = renderview.origin;
    const float3 rd = get_pixel_ray(uv, renderview.inv_view, renderview.inv_proj);
    // const float2 ground_point = ground_hit(ro, rd);
    // const float ground_t = ro.y / -rd.y;
    // const float3 ground_albedo = 0.90 + checkers(ground_point) * 0.1;

    const hit_result hit = trace_bvh(ro, rd, 1000.0, scene_bvh, scene_prims);
    if (hit.t >= 1000.0) {
        const float3 radiance = getSkyColor(rd, getAnimatedSunDir((float)frame_idx * 0.01666));

        out_albedo[thread_id] = float4(radiance, 1.0);
        out_normal_depth[thread_id] = float4(0.0, 0.0, 0.0, 2000.0);
        return;
    }
    
    { /* Get the normal of the hit primitive */ 
        float3 normal = scene_norms[hit.prim_i].n1.xyz * hit.uv.x;
        normal += scene_norms[hit.prim_i].n2.xyz * hit.uv.y;
        normal += scene_norms[hit.prim_i].n0.xyz * (1.0 - (hit.uv.x + hit.uv.y));

        // const float3 normal = normalize(n1 * w0 + n2 * w1 + n0 * w2);
        out_normal_depth[thread_id] = float4(normalize(normal), hit.t);
    }
    // out_normal_depth[thread_id] = float4(normalize(cross(scene_prims[hit.prim_i].v0.xyz - scene_prims[hit.prim_i].v1.xyz, scene_prims[hit.prim_i].v2.xyz - scene_prims[hit.prim_i].v0.xyz)) * float3(-1, -1, 1), hit.t);

    /* Write out to the output texture */
    const float3 material = scene_prims[hit.prim_i].get_color();
    const bool is_emissive = any(material > 0.0);
    if (is_emissive) {
        out_albedo[thread_id] = float4(aces(material), 0.5);
    } else {
        out_albedo[thread_id] = float4(-material, 1.0);
        // out_albedo[thread_id] = float4(-material * saturate(dot(float3(0.5773, 0.5773, 0.5773), out_normal_depth[thread_id].xyz)), 1.0);
    }
}
