#include "common.glsl"

#define VIEW_BASE_COLOR 0
#define VIEW_NORMAL     1
#define VIEW_ROUGHNESS  2
#define VIEW_METALLIC   3
#define VIEW_EMISSIVE   4

// ------------------------------------------------------------------------
// Set 0 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 0, binding = 0, std430) readonly buffer MaterialBuffer 
{
    Material data[];
} Materials;

layout (set = 0, binding = 1, std430) readonly buffer InstanceBuffer 
{
    Instance data[];
} Instances;

layout (set = 0, binding = 2, std430) readonly buffer LightBuffer 
{
    Light data[];
} Lights;

layout (set = 0, binding = 3) uniform accelerationStructureEXT u_TopLevelAS;

layout (set = 0, binding = 4) uniform samplerCube s_EnvironmentMap;

// ------------------------------------------------------------------------
// Set 1 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 1, binding = 0, std430) readonly buffer VertexBuffer 
{
    Vertex data[];
} Vertices[];

// ------------------------------------------------------------------------
// Set 2 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 2, binding = 0) readonly buffer IndexBuffer 
{
    uint data[];
} Indices[];

// ------------------------------------------------------------------------
// Set 3 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 3, binding = 0) readonly buffer SubmeshInfoBuffer 
{
    uvec2 data[];
} SubmeshInfo[];

// ------------------------------------------------------------------------
// Set 4 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 4, binding = 0) uniform sampler2D s_Textures[];

// ------------------------------------------------------------------------
// Set 5 ------------------------------------------------------------------
// ------------------------------------------------------------------------

#if defined(RAY_DEBUG_VIEW)
layout (set = 5, binding = 0) writeonly buffer DebugRayVertexBuffer_t 
{
    DebugRayVertex vertices[];
} DebugRayVertexBuffer;

layout (set = 5, binding = 1) buffer DebugRayDrawArgs_t
{
    uint count;
    uint instance_count;
    uint first;
    uint base_instance;
} DebugRayDrawArgs;
#else
layout(set = 5, binding = 0, rgba32f) readonly uniform image2D i_PreviousColor;
#endif

// ------------------------------------------------------------------------
// Set 6 ------------------------------------------------------------------
// ------------------------------------------------------------------------

#if !defined(RAY_DEBUG_VIEW)
layout(set = 6, binding = 0, rgba32f) writeonly uniform image2D i_CurrentColor;
#endif

// ------------------------------------------------------------------------
// Push Constants ---------------------------------------------------------
// ------------------------------------------------------------------------

layout(push_constant) uniform PathTraceConsts
{
    mat4 view_inverse;
    mat4 proj_inverse;
    ivec4 ray_debug_pixel_coord;
    float accumulation;
    uint num_lights;
    uint num_frames;
    uint debug_vis;
} u_PathTraceConsts;

// ------------------------------------------------------------------------
// Input Payload ----------------------------------------------------------
// ------------------------------------------------------------------------

rayPayloadInEXT DebugVisPayload p_DebugVisPayload;

// ------------------------------------------------------------------------
// Hit Attributes ---------------------------------------------------------
// ------------------------------------------------------------------------

hitAttributeEXT vec2 b_HitAttribs;

// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

Vertex get_vertex(uint mesh_idx, uint vertex_idx)
{
    return Vertices[nonuniformEXT(mesh_idx)].data[vertex_idx];
}

// ------------------------------------------------------------------------

HitInfo fetch_hit_info()
{
    uvec2 primitive_offset_mat_idx = SubmeshInfo[nonuniformEXT(gl_InstanceCustomIndexEXT)].data[gl_GeometryIndexEXT];

    HitInfo hit_info;

    hit_info.mat_idx = primitive_offset_mat_idx.y;
    hit_info.primitive_offset = primitive_offset_mat_idx.x;
    hit_info.primitive_id = gl_PrimitiveID;

    return hit_info;
}

// ------------------------------------------------------------------------

Triangle fetch_triangle(in Instance instance, in HitInfo hit_info)
{
    Triangle tri;

    uint primitive_id =  hit_info.primitive_id + hit_info.primitive_offset;

    uvec3 idx = uvec3(Indices[nonuniformEXT(instance.mesh_idx)].data[3 * primitive_id], 
                      Indices[nonuniformEXT(instance.mesh_idx)].data[3 * primitive_id + 1],
                      Indices[nonuniformEXT(instance.mesh_idx)].data[3 * primitive_id + 2]);

    tri.v0 = get_vertex(instance.mesh_idx, idx.x);
    tri.v1 = get_vertex(instance.mesh_idx, idx.y);
    tri.v2 = get_vertex(instance.mesh_idx, idx.z);

    return tri;
}

// ------------------------------------------------------------------------

void transform_vertex(in Instance instance, inout Vertex v)
{
    mat4 model_mat = instance.model_matrix;
    mat3 normal_mat = mat3(instance.normal_matrix);

    v.position = model_mat * v.position; 
    v.normal.xyz = normal_mat * v.normal.xyz;
    v.tangent.xyz = normal_mat * v.tangent.xyz;
    v.bitangent.xyz = normal_mat * v.bitangent.xyz;
}

// ------------------------------------------------------------------------

vec3 get_normal_from_map(vec3 tangent, vec3 bitangent, vec3 normal, vec2 tex_coord, uint normal_map_idx)
{
    // Create TBN matrix.
    mat3 TBN = mat3(normalize(tangent), normalize(bitangent), normalize(normal));

    // Sample tangent space normal vector from normal map and remap it from [0, 1] to [-1, 1] range.
    vec3 n = normalize(textureLod(s_Textures[nonuniformEXT(normal_map_idx)], tex_coord, 0.0).rgb * 2.0 - 1.0);

    // Multiple vector by the TBN matrix to transform the normal from tangent space to world space.
    n = normalize(TBN * n);

    return n;
}

// ------------------------------------------------------------------------

void fetch_albedo(in Material material, inout SurfaceProperties p)
{
    if (material.texture_indices0.x == -1)
        p.albedo = material.albedo;
    else
        p.albedo = textureLod(s_Textures[nonuniformEXT(material.texture_indices0.x)], p.vertex.tex_coord.xy, 0.0);
}

// ------------------------------------------------------------------------

void fetch_normal(in Material material, inout SurfaceProperties p)
{
    if (material.texture_indices0.y == -1)
        p.normal = p.vertex.normal.xyz;
    else
        p.normal = get_normal_from_map(p.vertex.tangent.xyz, p.vertex.bitangent.xyz, p.vertex.normal.xyz, p.vertex.tex_coord.xy, material.texture_indices0.y);
}

// ------------------------------------------------------------------------

void fetch_roughness(in Material material, inout SurfaceProperties p)
{
    if (material.texture_indices0.z == -1)
        p.roughness = material.roughness_metallic.r;
    else
        p.roughness = textureLod(s_Textures[nonuniformEXT(material.texture_indices0.z)], p.vertex.tex_coord.xy, 0.0)[material.texture_indices1.z];
}

// ------------------------------------------------------------------------

void fetch_metallic(in Material material, inout SurfaceProperties p)
{
    if (material.texture_indices0.w == -1)
        p.metallic = material.roughness_metallic.g;
    else
        p.metallic = textureLod(s_Textures[nonuniformEXT(material.texture_indices0.w)], p.vertex.tex_coord.xy, 0.0)[material.texture_indices1.w];
}

// ------------------------------------------------------------------------

void fetch_emissive(in Material material, inout SurfaceProperties p)
{
    if (material.texture_indices1.x == -1)
        p.emissive = material.emissive.rgb;
    else
        p.emissive = textureLod(s_Textures[nonuniformEXT(material.texture_indices1.x)], p.vertex.tex_coord.xy, 0.0).rgb;
}

// ------------------------------------------------------------------------

void populate_surface_properties(out SurfaceProperties p)
{
    const Instance instance = Instances.data[gl_InstanceCustomIndexEXT];
    const HitInfo hit_info = fetch_hit_info();
    const Triangle triangle = fetch_triangle(instance, hit_info);
    const Material material = Materials.data[hit_info.mat_idx];

    p.vertex = interpolated_vertex(triangle, b_HitAttribs);
    
    transform_vertex(instance, p.vertex);

    fetch_albedo(material, p);
    fetch_normal(material, p);
    fetch_roughness(material, p);
    fetch_metallic(material, p);
    fetch_emissive(material, p);

    p.roughness = max(p.roughness, MIN_ROUGHNESS);

    p.F0 = mix(vec3(0.03), p.albedo.xyz, p.metallic);
    p.alpha = p.roughness * p.roughness;
    p.alpha2 = p.alpha * p.alpha;
}


// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
    SurfaceProperties p;

    const Instance instance = Instances.data[gl_InstanceCustomIndexEXT];
    const HitInfo hit_info = fetch_hit_info();
    const Triangle triangle = fetch_triangle(instance, hit_info);
    const Material material = Materials.data[hit_info.mat_idx];

    p.vertex = interpolated_vertex(triangle, b_HitAttribs);
    
    transform_vertex(instance, p.vertex);

#if defined(RAY_DEBUG_VIEW)
    // Skip the primary ray
    uint debug_ray_vert_idx = atomicAdd(DebugRayDrawArgs.count, 2);

    DebugRayVertex v0;

    v0.position = vec4(gl_WorldRayOriginEXT, 1.0f);
    v0.color = vec4(p_DebugVisPayload.debug_color, 1.0f);

    DebugRayVertex v1;

    v1.position = vec4(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT  * gl_HitTEXT, 1.0f);
    v1.color = vec4(p_DebugVisPayload.debug_color, 1.0f);

    DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 0] = v0;
    DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 1] = v1;
#endif

    if (u_PathTraceConsts.debug_vis == VIEW_BASE_COLOR)
    {
        fetch_albedo(material, p);
        p_DebugVisPayload.color = p.albedo.rgb;
    } 
    if (u_PathTraceConsts.debug_vis == VIEW_NORMAL)
    {
        fetch_normal(material, p);
        p_DebugVisPayload.color = p.normal.rgb;
    } 
    if (u_PathTraceConsts.debug_vis == VIEW_ROUGHNESS)
    {
        fetch_roughness(material, p);
        p_DebugVisPayload.color = vec3(p.roughness);
    } 
    if (u_PathTraceConsts.debug_vis == VIEW_METALLIC)
    {
        fetch_metallic(material, p);
        p_DebugVisPayload.color = vec3(p.metallic);
    } 
    if (u_PathTraceConsts.debug_vis == VIEW_EMISSIVE)
    {
        fetch_emissive(material, p);
        p_DebugVisPayload.color = p.emissive.rgb;
    } 
}

// ------------------------------------------------------------------------