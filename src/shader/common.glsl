#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "random.glsl"

#define PATH_TRACE_CLOSEST_HIT_SHADER_IDX 0
#define PATH_TRACE_MISS_SHADER_IDX 0

#define VISIBILITY_CLOSEST_HIT_SHADER_IDX 1
#define VISIBILITY_MISS_SHADER_IDX 1

#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT 1
#define LIGHT_POINT 2
#define LIGHT_ENVIRONMENT_MAP 3
#define LIGHT_AREA 4

#define M_PI 3.14159265359
#define EPSILON 0.0001f
#define INFINITY 100000.0f
#define MIN_ROUGHNESS 0.1f
#define RADIANCE_CLAMP_COLOR vec3(1.0f)

#define MAX_RAY_BOUNCES 5

struct PathTracePayload
{
    vec3 L;
    vec3 T;
    uint depth;
    RNG rng;
#if defined(RAY_DEBUG_VIEW)
    vec3 debug_color;
#endif
};

struct DebugVisPayload
{
    vec3 color;
#if defined(RAY_DEBUG_VIEW)
    vec3 debug_color;
#endif
};

struct Vertex
{
    vec4 position;
    vec4 tex_coord;
    vec4 normal;
    vec4 tangent;
    vec4 bitangent;
};

struct DebugRayVertex
{
    vec4 position;
    vec4 color;
};

struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

struct SurfaceProperties
{
    Vertex vertex;
    vec4 albedo;
    vec3 emissive;
    vec3 normal;
    vec3 F0;
    float metallic;
    float roughness;   
    float alpha;
    float alpha2; 
};

struct Material
{
    ivec4 texture_indices0; // x: albedo, y: normals, z: roughness, w: metallic
    ivec4 texture_indices1; // x: emissive, z: roughness_channel, w: metallic_channel
    vec4  albedo;
    vec4  emissive;
    vec4  roughness_metallic;
};

struct Light
{
    vec4 light_data0; // x: light type, yzw: color
    vec4 light_data1; // xyz: direction, w: intensity
    vec4 light_data2; // x: range, y: cone angle
};

struct HitInfo
{
    uint mat_idx;
    uint primitive_offset;
    uint primitive_id;
};

struct Instance
{
    mat4 model_matrix;
    mat4 normal_matrix;
    uint mesh_idx;
};


// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

bool is_nan(vec3 c)
{
    return isnan(c.x) || isnan(c.y) || isnan(c.z);
}

// ------------------------------------------------------------------------

bool is_black(vec3 c)
{
    return c.x == 0.0f && c.y == 0.0f && c.z == 0.0f;
}

// ------------------------------------------------------------------------

Vertex interpolated_vertex(in Triangle tri, in vec2 hit_attribs)
{;
    const vec3 barycentrics = vec3(1.0 - hit_attribs.x - hit_attribs.y, hit_attribs.x, hit_attribs.y);

    Vertex o;

    o.position = vec4(tri.v0.position.xyz * barycentrics.x + tri.v1.position.xyz * barycentrics.y + tri.v2.position.xyz * barycentrics.z, 1.0);
    o.tex_coord.xy = tri.v0.tex_coord.xy * barycentrics.x + tri.v1.tex_coord.xy * barycentrics.y + tri.v2.tex_coord.xy * barycentrics.z;
    o.normal.xyz = normalize(tri.v0.normal.xyz * barycentrics.x + tri.v1.normal.xyz * barycentrics.y + tri.v2.normal.xyz * barycentrics.z);
    o.tangent.xyz = normalize(tri.v0.tangent.xyz * barycentrics.x + tri.v1.tangent.xyz * barycentrics.y + tri.v2.tangent.xyz * barycentrics.z);
    o.bitangent.xyz = normalize(tri.v0.bitangent.xyz * barycentrics.x + tri.v1.bitangent.xyz * barycentrics.y + tri.v2.bitangent.xyz * barycentrics.z);

    return o;
}

// ------------------------------------------------------------------------

#endif