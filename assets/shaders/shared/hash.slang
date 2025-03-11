module hash;

import fast_math;

/** @brief 1D PCG hash function. */
public inline uint pcg1d(const uint seed) {
    const uint state = seed * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/** @brief 3D PCG hash function. */
public inline uint3 pcg3d(const uint3 seed) {
    const uint3 state = seed * 747796405u + 2891336453u;
    const uint3 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/* XXHash32 <https://jcgt.org/published/0009/03/02/> */
public inline uint xxhash32(const uint p) {
    const uint PRIME32_2 = 2246822519u, PRIME32_3 = 3266489917u;
    const uint PRIME32_4 = 668265263u, PRIME32_5 = 374761393u;
    uint h32 = p + PRIME32_5;
    h32 = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
    h32 = PRIME32_2 * (h32 ^ (h32 >> 15u));
    h32 = PRIME32_3 * (h32 ^ (h32 >> 13u));
    return h32 ^ (h32 >> 16u);
}

/* Hash grid settings (XYZ9L5) */
static const uint HG_POS_BITS = 9u, HG_LVL_BITS = 5u;
static const uint HG_POS_MAX = (1u << HG_POS_BITS) - 0u; /* inclusive */
static const uint HG_LVL_MAX = (1u << HG_LVL_BITS) - 1u; /* exclusive */
static const uint HG_LVL_BIAS = 2u;

/* Based on a Surfel position and the scene origin, determine the grid level of the Surfel. */
inline uint surfel_grid_level(const float3 pos, const float3 scene_origin) {
    const float dst_sqr = distance2(pos, scene_origin);
    return clamp(HG_LVL_BIAS + (uint)(log(dst_sqr) * 0.7213477), 0u, HG_LVL_MAX);
}

/* Find the world-space size of a grid cell in units based on the grid level and scene scale. */
public inline float surfel_grid_size(const uint level, const float scene_scale) {
    return (float)(1u << level) / (scene_scale * 4.0);
}

/* Find the hash grid location of a Surfel based on the scene origin and scale. */
public inline uint4 surfel_grid_location(const float3 pos, const float3 scene_origin, const float scene_scale) {
    const uint grid_level = surfel_grid_level(pos, scene_origin);
    const float grid_size = surfel_grid_size(grid_level, scene_scale);
    return uint4(asuint((int3)floor(pos / grid_size)), grid_level);
}

/* Hash a Surfel grid location. */
public inline uint surfel_hash_function(const uint3 loc, const uint level) {
    return xxhash32(((loc.x % HG_POS_MAX) << (HG_POS_BITS * 0u))
                  | ((loc.y % HG_POS_MAX) << (HG_POS_BITS * 1u))
                  | ((loc.z % HG_POS_MAX) << (HG_POS_BITS * 2u))
                  | ((level & HG_LVL_MAX) << (HG_POS_BITS * 3u)));
}

/* Get the hash key of a Surfel at a given position. */
public inline uint surfel_cell_hash(const float3 pos, const float3 origin, const float scene_scale) {
    const uint4 grid_loc = surfel_grid_location(pos, origin, scene_scale);
    return surfel_hash_function(grid_loc.xyz, grid_loc.w);
}

/* Find the minimum & maximum grid level of a Surfel. */
public inline uint2 surfel_grid_level_minmax(const float3 pos, const float radius, const float3 origin, const float scene_scale) {
    const float3 dir = normalize(origin - pos) * radius;
    return uint2(surfel_grid_level(pos - dir, origin), surfel_grid_level(pos + dir, origin));
}

/* Find the position of a Surfel in a given grid level. */
public inline int3 surfel_grid_position(const float3 pos, const float grid_size) {
    return (int3)floor(pos / grid_size);
}
