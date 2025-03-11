module octahedral;

/* Octahedral mapping functions */
/* Source: <https://jcgt.org/published/0003/02/01/paper.pdf> */

inline float2 sign_nzero(const float2 v) {
    return float2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}

/* Encode a direction as UV *(-1.0 -> 1.0)* coordinates. */
public inline float2 oct_encode(const float3 v) {
    const float2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * sign_nzero(p)) : p;
}

/* Decode UV *(-1.0 -> 1.0)* coordinates into a direction. */
public inline float3 oct_decode(const float2 e) {
    // float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    // if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign_nzero(v.xy);
    // return normalize(v);

    /* Equal-Area Mapping <https://pbr-book.org/4ed/Geometry_and_Transformations/Spherical_Geometry#x3-Equal-AreaMapping> */
    const float2 v = abs(e);
    const float sdist = 1.0 - (v.x + v.y);
    const float r = 1.0 - abs(sdist);
    const float phi = ((v.y - v.x) / r + 1.0) * 0.785398;
    const float r_sqr = r * r;
    const float z = copysign(1.0 - r_sqr, sdist);
    const float cos_phi = copysign(cos(phi), e.x);
    const float sin_phi = copysign(sin(phi), e.y);
    const float r_scl = r * sqrt(2.0 - r_sqr);
    return float3(cos_phi * r_scl, sin_phi * r_scl, z);
}
