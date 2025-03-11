module camera;

/* Camera descriptor. */ 
public struct camera_t { public float4x4 inv_vp; public float3 origin; };

/* Rendering view descriptor. */
public struct renderview_t {
    /* View matrix. */
    public float4x4 view;
    /* Projection matrix. */
    public float4x4 proj;
    /* Inverse view matrix. */
    public float4x4 inv_view;
    /* Inverse projection matrix. */
    public float4x4 inv_proj;
    /* World-space origin of the view. */
    public float3 origin;
    /* Field of view in radians. */
    public float fov;
}

/** @brief Get the ray direction for a given UV coordinate on screen. */
public float3 get_pixel_ray(const float2 uv, const float4x4 inv_view, const float4x4 inv_proj) {
    /* UV to NDS */
    float2 px_NDS = uv * 2.0 - 1.0;
    px_NDS.y = -px_NDS.y; /* flip Y */
    const float3 point_NDS = float3(px_NDS, -1.0);
    const float4 point_NDSH = float4(point_NDS, 1.0);
    float4 eye = mul(inv_proj, point_NDSH);
    eye.w = 0.0;

    /* World direction */
    const float3 world_dir = mul(inv_view, eye).xyz;
    // const float3 world_dir = mul(point_NDSH, inv_vp).xyz;
    return normalize(world_dir); 
}
