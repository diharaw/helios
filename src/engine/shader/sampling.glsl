#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

#include "common.glsl"

float next_float(inout RNG rng)
{
    uint u = 0x3f800000 | (rng_next(rng) >> 9);
    return uintBitsToFloat(u) - 1.0;
}

uint next_uint(inout RNG rng, uint nmax)
{
    float f = next_float(rng);
    return uint(floor(f * nmax));
}

vec2 next_vec2(inout RNG rng)
{
    return vec2(next_float(rng), next_float(rng));
}

vec3 next_vec3(inout RNG rng)
{
    return vec3(next_float(rng), next_float(rng), next_float(rng));
}

mat3 make_rotation_matrix(vec3 z)
{
    const vec3 ref = abs(dot(z, vec3(0, 1, 0))) > 0.99f ? vec3(0, 0, 1) : vec3(0, 1, 0);

    const vec3 x = normalize(cross(ref, z));
    const vec3 y = cross(z, x);

    return mat3(x, y, z);
}

#endif