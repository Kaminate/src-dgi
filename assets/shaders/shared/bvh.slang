module bvh;

/* GPU optimized BVH node. */
public struct basic_node {
    /* xyz = (float3) left_min  | w = (u32) left */
    public float4 lmin;
    /* xyz = (float3) left_max  | w = (u32) right */
    public float4 lmax;
    /* xyz = (float3) right_min | w = (u32) prim_index */
    public float4 rmin;
    /* xyz = (float3) right_max | w = (u32) prim_count */
    public float4 rmax;

    public inline uint left_index() { return asuint(lmin.w); }
    public inline uint right_index() { return asuint(lmax.w); }
    public inline uint prim_index() { return asuint(rmin.w); }
    public inline uint prim_count() { return asuint(rmax.w); }

    /** @returns True if this node a leaf node. */
    public inline bool is_leaf() { return prim_count() > 0; }
}

/* Triangle primitive. */
public struct basic_tri {
    public float4 v0;
    public float4 v1;
    public float4 v2;

    public inline float3 get_color() { return float3(v0.w, v1.w, v2.w); }
}

/* Normals for a triangle primitive. */
public struct basic_nor {
    public float4 n0;
    public float4 n1;
    public float4 n2;
}

/* Ray traversal hit result. */
public struct hit_result {
    public float t;
    public uint prim_i;
    public float2 uv;
}

/** @brief Ray to Sphere intersection test. */
public inline float ray_sphere(float3 ro, float3 rd, float3 center, float radius) {
    const float3 l = center - ro;
    const float tca = dot(l, rd);
    const float d2 = dot(l, l) - tca * tca;
    if (d2 > radius * radius) return 1e30; /* miss */
    const float thc = sqrt(radius * radius - d2);
    const float t0 = tca - thc;
    const float t1 = tca + thc;
    if (t0 < 0.0) return 1e30; /* miss */
    return t0;
}

/** @brief Ray to AABB intersection test. */
public inline float ray_aabb(float3 ro, float3 ird, float3 bbmin, float3 bbmax) {
    float tmin = 0.0, tmax = 1e30;

    /* Loop will be unrolled */
    [unroll] for (int axis = 0; axis < 3; ++axis) {
        const float t1 = (bbmin[axis] - ro[axis]) * ird[axis];
        const float t2 = (bbmax[axis] - ro[axis]) * ird[axis];

        const float dmin = min(t1, t2);
        const float dmax = max(t1, t2);

        tmin = max(dmin, tmin);
        tmax = min(dmax, tmax);
    }

    if (tmax >= tmin) return tmin;
    return 1e30; /* miss */
}

/** @brief Ray to Triangle intersection test. */
public inline float3 ray_triangle(const float3 ro, const float3 rd, const basic_tri tri) {
    const float3 edge1 = tri.v1.xyz - tri.v0.xyz, edge2 = tri.v2.xyz - tri.v0.xyz;
    const float3 h = cross(rd, edge2);
    const float a = dot(edge1, h);
    if (abs(a) < 0.0000001) return 1e30;
    const float f = 1.0 / a;
    const float3 s = ro - tri.v0.xyz;
    const float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return 1e30;
    const float3 q = cross(s, edge1);
    const float v = f * dot(rd, q);
    if (v < 0.0 || u + v > 1.0) return 1e30;
    const float d = f * dot(edge2, q);
    if (d < 0.0f) return 1e30;
    return float3(u, v, d);
}

/** 
 * @brief Ray to BVH intersection test. 
 * Credit: <https://github.com/jbikker/tinybvh>
 */
public [ForceInline] hit_result trace_bvh(float3 ro, float3 rd, float tmax, StructuredBuffer<basic_node> nodes, StructuredBuffer<basic_tri> prims) {
    uint node_ptr = 0, stack[32], stack_ptr = 0;
    float mind = tmax;
    float2 hit_uv = float2(0.0, 0.0);
    uint hit_prim = 0;
    const float3 ird = 1.0 / rd;

    for (;;) {
        const uint prim_count = nodes[node_ptr].prim_count();

        /* Check if this node is a leaf node */
        if (prim_count > 0) {
            const uint prim_index = nodes[node_ptr].prim_index();

            /* Check if we hit any primitives */
            for (uint i = 0; i < prim_count; ++i) {
                const float3 v0 = prims[prim_index + i].v0.xyz;
                const float3 edge1 = prims[prim_index + i].v1.xyz - v0; 
                const float3 edge2 = prims[prim_index + i].v2.xyz - v0;
                const float3 s = ro - v0;
                
                 /* Triangle intersection test */
                const float3 h = cross(rd, edge2);
                const float a = dot(edge1, h);
                if (abs(a) < 0.0000001) continue;
                const float f = 1.0 / a;
                const float u = f * dot(s, h);
                if (u < 0.0 || u > 1.0) continue;
                const float3 q = cross(s, edge1);
                const float v = f * dot(rd, q);
                if (v < 0.0 || u + v > 1.0) continue;
                const float d = f * dot(edge2, q);
                if (d < 0.0) continue;

                /* Record a new closest hit */
                if (d < mind) {
                    mind = d;
                    hit_prim = prim_index + i;
                    hit_uv = float2(u, v);
                }
            }

            /* Decend down the stack */
            if (stack_ptr == 0) break;
            node_ptr = stack[--stack_ptr];
            continue;
        }

        /* Check if our ray hits either or both of the child nodes */
        float dist1, dist2;
        uint left = nodes[node_ptr].left_index(), right = nodes[node_ptr].right_index();
        {
            const float3 t1a = (nodes[node_ptr].lmin.xyz - ro) * ird, t2a = (nodes[node_ptr].lmax.xyz - ro) * ird;
            const float3 t1b = (nodes[node_ptr].rmin.xyz - ro) * ird, t2b = (nodes[node_ptr].rmax.xyz - ro) * ird;
            const float3 minta = min(t1a, t2a), maxta = max(t1a, t2a);
            const float3 mintb = min(t1b, t2b), maxtb = max(t1b, t2b);
            const float tmina = max(max3(minta.x, minta.y, minta.z), 0.0);
            const float tminb = max(max3(mintb.x, mintb.y, mintb.z), 0.0);
            const float tmaxa = min(min3(maxta.x, maxta.y, maxta.z), mind);
            const float tmaxb = min(min3(maxtb.x, maxtb.y, maxtb.z), mind);
            dist1 = tmina > tmaxa ? 1e30 : tmina;
            dist2 = tminb > tmaxb ? 1e30 : tminb;
        }

        /* Child to be traversed first should be the closest one */
        if (dist1 > dist2) {
			float h = dist1; dist1 = dist2; dist2 = h;
			uint t = left; left = right; right = t;
		}
        
        /* Traverse child nodes if they were intersected */
		if (dist1 > tmax) {
			if (stack_ptr == 0) break; 
            else node_ptr = stack[--stack_ptr];
		} else {
			node_ptr = left;
			if (dist2 <= tmax) stack[stack_ptr++] = right;
		}
    }

    return {mind, hit_prim, hit_uv};
}
