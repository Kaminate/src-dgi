module fast_math;

public static const float PI = 3.1415926535897932384626433832795;
/* Reciprocal PI `(1.0 / PI)` */
public static const float RPI = 1.0 / PI;
public static const float PI2 = 6.283185307179586476925286766559;
public static const float PI4 = 12.56637061435917295385057353312;

/* Source: <https://github.com/michaldrobot/ShaderFastLibs> */
/* Fast reciprocal using IEEE int approximation. */
public [ForceInline] float fast_rcp(const float x) {
    const int y = asint(x); 
    const int z = 0x7EF311C2 - y;
    return asfloat(z);
}

/* Source: <https://github.com/michaldrobot/ShaderFastLibs> */
/* Fast square root using IEEE int approximation. */
public [ForceInline] float fast_sqrt(const float x) {
    const int y = asint(x); 
    const int z = 0x1FBD1DF5 + (y >> 1);
    return asfloat(z);
}

/* `log(x) / log(base)` */
public [ForceInline] float logbase(const float x, const float base) {
    return log(x) / log(base);
}

/* Distance squared. */
public [ForceInline] float distance2(const float3 a, const float3 b) {
    const float3 c = a - b;
    return dot(c, c);
}

public [ForceInline] float max3(const float x, const float y,  const float z) { return max(x, max(y, z)); }
public [ForceInline] float min3(const float x, const float y,  const float z) { return min(x, min(y, z)); }
