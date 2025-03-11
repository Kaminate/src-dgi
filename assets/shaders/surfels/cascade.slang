module cascade;

/* Surface area of the unit sphere. */
static const float PI4 = 12.56637;

/* TODO: Perhaps branch factors could become parameters as well? */
/* Branch factor: Sx/Ax */
static const uint SPATIAL_FACTOR = 4; /* Spatial branch factor. */
static const uint ANGULAR_FACTOR = 4; /* Angular branch factor. */

/* Get the spatial scaling value for a cascade. */
inline uint spatial_scale(const uint cascade_index) { return (uint)pow(SPATIAL_FACTOR, cascade_index); }

/* WARN: Only works if the branch factor can be divided by 2. */
/* Get half the spatial scaling value for a cascade. */
inline uint half_spatial_scale(const uint cascade_index) { return (uint)pow(SPATIAL_FACTOR >> 1u, cascade_index); }

/* Get the angular scaling value for a cascade. */
inline uint angular_scale(const uint cascade_index) { return (uint)pow(ANGULAR_FACTOR, cascade_index); }

/* WARN: Only works if the branch factor can be divided by 2. */
/* Get half the angular scaling value for a cascade. */
inline uint half_angular_scale(const uint cascade_index) { return (uint)pow(ANGULAR_FACTOR >> 1u, cascade_index); }

/* Surfel Cascade context. *(4 bytes)* */
public struct context_t {
    /* Packing: `MS16b` Frame Index, `LS16b` Cascade Index */
    public uint data; 

    /* Get the index of the cascade in the current context. */
    public inline uint get_cascade_index() { return data & 0xFFFFu; }
    /* Get the index of the frame in the current context. *(useful for random generation)* */
    public inline uint get_frame_index() { return data >> 16u; }
}

/* Surfel Cascade parameters. *(36 bytes)* */
public struct cascade_t {
    /* `[c0]` Capacity of the hash grid structure. */
    public uint c0_grid_capacity;
    /* `[c0]` Scale of grid cells in the hash grid structure. */
    public float c0_grid_scale;
    /* `[cN]` Surfel acceleration structure grid cell Surfel capacity. */
    public uint cell_capacity;
    /* NOTE: Radiance data is stored in a texture, e.g. 16 intervals = 4x4 square. */
    /* `[c0]` Square root of the number of intervals per Surfel probe. */
    public uint c0_memory_width;
    /* `[c0]` Maximum number of active Surfel probes. */
    public uint c0_probe_capacity;
    /* `[c0]` Surfel probe radius in screen-space. */
    public float c0_probe_radius;
    /* `[cN]` Maximum projected solid angle of intervals. *(used to derive interval length)* */
    public float max_solid_angle;
    
    /* `[c0]` Number of intervals per Surfel probe. */
    property uint c0_interval_count { get { return c0_memory_width * c0_memory_width; } }

    /* `[cN]` Get the square root of the number of intervals per Surfel probe. */
    public inline uint get_memory_width(const uint cascade_index) 
    { return c0_memory_width * half_angular_scale(cascade_index); }
    
    /* `[cN]` Get the maximum number of active Surfel probes. */
    public inline uint get_probe_capacity(const uint cascade_index) 
    { return c0_probe_capacity / spatial_scale(cascade_index); }

    /* `[cN]` Get the bounds of the Surfel hash grid list. */
    public inline uint get_list_bounds(const uint cascade_index)
    { return get_probe_capacity(cascade_index) * 16u; }
    
    /* `[cN]` Get the Surfel probe radius in screen-space. */
    public inline float get_probe_radius(const uint cascade_index) 
    { return c0_probe_radius * half_spatial_scale(cascade_index + 1u); }
    
    /* `[cN]` Get the minimum Surfel probe radius in screen-space. */
    public inline float get_probe_min_radius(const uint cascade_index) 
    { return (c0_probe_radius * half_spatial_scale(cascade_index + 1u)); }
    
    /* `[cN]` Get the number of intervals per Surfel probe. */
    public inline uint get_interval_count(const uint cascade_index) 
    { return c0_interval_count * angular_scale(cascade_index); }
    
    /* `[cN]` Get the start *(x)* & end *(y)* time for a Cascade interval. */
    public inline float2 get_interval(const uint cascade_index) 
    { return base_interval_length(max_solid_angle, c0_interval_count) * interval_scale(cascade_index); }

    /* `[cN]` Get the capacity of the hash grid structure. */
    public inline uint get_grid_capacity(const uint cascade_index)
    { return c0_grid_capacity; /* / half_spatial_scale(cascade_index); */ }
    
    /* `[cN]` Get the scale of grid cells in the hash grid structure. */
    public inline float get_grid_scale(const uint cascade_index)
    { return c0_grid_scale / half_spatial_scale(cascade_index); }
    
    /* `[cN]` Size of a single Surfel acceleration structure grid cell in u32. */
    public property uint cell_stride { get { return 1u + cell_capacity; } }
}

/* Merge a far radiance interval into a near one. */
public inline float4 merge_intervals(const float4 near, const float4 far) {
    /* Only carry far radiance if near visibility is `1.0`. */
    const float3 radiance = near.xyz + (near.w * far.xyz);
    /* Always carry a visibility of `0.0`. */
    const float visibility = near.w * far.w;

    return float4(radiance, visibility);
}

/* Derive the optimal base interval length from cascade parameters. */
inline float base_interval_length(const float max_solid_angle, const uint c0_count) {
    /* 4PI is the surface area of a unit sphere. */
    return max_solid_angle * c0_count / PI4;
}

/* Get the start *(x)* & end *(y)* scale for a cascade interval. */
inline float2 interval_scale(const uint cascade_index) {
    /* Early out for `[c0]` */
    if (cascade_index == 0u) return float2(0.0, ANGULAR_FACTOR);

    /* Get the start scale with respect to the branch factor. */
    const float start = pow(ANGULAR_FACTOR, cascade_index);
    return float2(start, start * ANGULAR_FACTOR);
}

static const float HDR_MAPPING_GAMMA = 1.4;
static const float HDR_RANGE = 512.0;

/* Encode a radiance interval for the radiance cache. */
public inline float4 encode_interval(const float4 interval) {
    return interval; 
    // const float3 c = pow(interval.rgb * (1.0 / HDR_RANGE), 1.0 / HDR_MAPPING_GAMMA);
    // return float4(c, interval.w);
}

/* Decode a radiance interval from the radiance cache. */
public inline float4 decode_interval(const float4 raw) {
    return raw; 
    // const float3 c = pow(raw.rgb, HDR_MAPPING_GAMMA) * HDR_RANGE;
    // return float4(c, raw.w);
}
