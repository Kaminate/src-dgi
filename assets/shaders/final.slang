/* Vertex output to pixel input */
struct v2p {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

[[vk::binding(1, 0)]] Sampler2D g_albedo;
[[vk::binding(2, 0)]] Sampler2D g_normal_depth;

[shader("vertex")] /* Vertex shader entry point */
v2p entry_vertex(int vertex_id : SV_VertexID) {
    const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
	return { float4(uv * 2.0 - 1.0, 0.0, 1.0), uv };
}

[shader("pixel")] /* Pixel shader entry point */
float4 entry_pixel(v2p input) : SV_Target {
    return g_albedo.Sample(input.uv);
    // return float4(g_normal_depth.Sample(input.uv).rgb, 1.0);
    // const float depth = g_normal_depth.Sample(input.uv).a * 0.1;
    // return float4(depth, depth, depth, 1.0);
}
