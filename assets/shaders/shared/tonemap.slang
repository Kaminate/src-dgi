module tonemap;

inline float3 rrt_and_odtfit(const float3 v) {
    const float3 a = v * (v + 0.0245786) - 0.000090537;
    const float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

/* ACES fitted tonemapping. */
public float3 aces_fitted(float3 color) {
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    const float3x3 rgb_to_rrt = float3x3(
        float3(0.59719, 0.35458, 0.04823),
        float3(0.07600, 0.90834, 0.01566),
        float3(0.02840, 0.13383, 0.83777)    
    );

    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    const float3x3 odt_to_rgb = float3x3(
        float3(1.60475, -0.53108, -0.07367),
        float3(-0.10208, 1.10813, -0.00605),
        float3(-0.00327, -0.07276, 1.07602)
    );

    color = mul(color, rgb_to_rrt);
    color = rrt_and_odtfit(color);
    color = mul(color, odt_to_rgb);

    return saturate(color);
}

/* ACES tonemapping. */
public inline float3 aces(const float3 x) {
    static const float a = 2.51;
    static const float b = 0.03;
    static const float c = 2.43;
    static const float d = 0.59;
    static const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

public inline float3 gamma_correct(const float3 x) {
    return pow(x, 1.0 / 2.2);
}
