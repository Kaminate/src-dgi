module shared;

import fast_math;

/* Get the index to a given cell position in the grid. */
public inline uint get_cell_ptr(const int3 cell, const int grid_w, const int grid_h) {
    return cell.z * grid_h * grid_w + cell.y * grid_w + cell.x;
}

/* Convert a world-space position to grid-space. */
public inline int3 world_to_grid(const float3 pos, const float3 origin, const int3 grid_size, const float cpu) {
    const float3 floating_cell = pos * cpu; // (pos - origin) * cpu;
    const int3 signed_cell = (int3)floor(floating_cell);
    return signed_cell + grid_size / 2;
}

/* Convert a world-space position to grid-space base position of 2x2x2. */
public inline int3 world_to_grid_base(const float3 pos, const float3 origin, const int3 grid_size, const float cpu) {
    const float3 floating_cell = pos * cpu + grid_size / 2 - 0.5; // (pos - origin) * cpu + grid_size / 2 - 0.5;
    return (int3)floor(floating_cell);
}

/* How much coverage does a Surfel have for this location.
   Anything below 0.0 lies within the Surfel, anything above lies outside. */
public inline float point_coverage(const float3 point, const float3 normal, const float4 posr, const float4 norw) {
    // const float pd = distance2(posr.xyz, point);
    // const float r2 = posr.w * (1.0 * 1.0); /* <- increase the radius to avoid overlap */
    // const float nd = dot(norw.xyz, normal);
    // return saturate((r2 - pd) / r2 - (1.0 - nd * nd));

    const float sqr_dist = distance2(posr.xyz, point);
    const float nor_dist = sqr_dist / posr.w - 1.0;
    // const float nor_fact = dot(norw.xyz, normal) - 1.0;
    // const float dist = distance(posr.xyz, point);
    // const float nor_dist = dist / sqrt(posr.w) - 1.0;
    return nor_dist; // + nor_fact * nor_fact;
}

/* Returns true if the point lies within the radius of the Surfel. */
public inline bool draw_surfel(const float3 point, const float4 posr) {
    const float pd = distance2(posr.xyz, point);
    return posr.w - pd > 0.0;
}
