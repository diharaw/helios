#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.glsl"

// ------------------------------------------------------------------------
// Set 0 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform PerFrameUBO
{
    mat4 view_inverse;
    mat4 proj_inverse;
    mat4 view;
    mat4 projection;
    vec4 cam_pos;
} u_PerFrameUBO;

layout (set = 0, binding = 1, std430) readonly buffer MaterialBuffer 
{
    Material data[];
} Materials;

layout (set = 0, binding = 2, std430) readonly buffer LightBuffer 
{
    Light data[];
} Lights;

layout (set = 0, binding = 3) uniform accelerationStructureNV u_TopLevelAS;

layout (set = 0, binding = 4, std430) readonly buffer VertexBuffer 
{
    Vertex vertices[];
} VertexArray[];

layout (set = 0, binding = 5) readonly buffer IndexBuffer 
{
    uint indices[];
} IndexArray[];

layout (set = 0, binding = 6) readonly buffer InstanceBuffer 
{
    mat4 model;
    uint indices[];
} InstanceArray[];

layout (set = 0, binding = 7) uniform sampler2D s_Textures[];

// ------------------------------------------------------------------------
// Push Constants ---------------------------------------------------------
// ------------------------------------------------------------------------

layout(push_constant) uniform PathTraceConsts
{
    uvec4 num_lights; // x: directional lights, y: point lights, z: spot lights, w: area lights  
    float accumulation;
    uint num_frames;
} u_PathTraceConsts;

// ------------------------------------------------------------------------
// Payload ----------------------------------------------------------------
// ------------------------------------------------------------------------

layout(location = 0) rayPayloadInNV PathTracePayload ray_payload;

// ------------------------------------------------------------------------
// Hit Attributes ---------------------------------------------------------
// ------------------------------------------------------------------------

hitAttributeNV vec3 hit_attribs;

// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

Vertex get_vertex(uint mesh_idx, uint vertex_idx)
{
    return VertexArray[nonuniformEXT(mesh_idx)].vertices[vertex_idx];
}

// ------------------------------------------------------------------------

Triangle fetch_triangle()
{
    Triangle tri;

    uint mesh_idx = InstanceArray[nonuniformEXT(gl_InstanceCustomIndexNV)].indices[0];

    uvec3 idx = uvec3(IndexArray[nonuniformEXT(mesh_idx)].indices[3 * gl_PrimitiveID], 
                      IndexArray[nonuniformEXT(mesh_idx)].indices[3 * gl_PrimitiveID + 1],
                      IndexArray[nonuniformEXT(mesh_idx)].indices[3 * gl_PrimitiveID + 2]);

    tri.v0 = get_vertex(mesh_idx, idx.x);
    tri.v1 = get_vertex(mesh_idx, idx.y);
    tri.v2 = get_vertex(mesh_idx, idx.z);

    tri.mat_idx = InstanceArray[nonuniformEXT(mesh_idx)].indices[uint(tri.v0.position.w) + 1];

    return tri;
}

// ------------------------------------------------------------------------

Vertex interpolated_vertex(in Triangle tri)
{
    mat4 model_mat = InstanceArray[nonuniformEXT(gl_InstanceCustomIndexNV)].model;
    mat3 normal_mat = mat3(model_mat);

    const vec3 barycentrics = vec3(1.0 - hit_attribs.x - hit_attribs.y, hit_attribs.x, hit_attribs.y);

    Vertex o;

    o.position = model_mat * vec4(tri.v0.position.xyz * barycentrics.x + tri.v1.position.xyz * barycentrics.y + tri.v2.position.xyz * barycentrics.z, 1.0);
    o.tex_coord.xy = tri.v0.tex_coord.xy * barycentrics.x + tri.v1.tex_coord.xy * barycentrics.y + tri.v2.tex_coord.xy * barycentrics.z;
    o.normal.xyz = normal_mat * normalize(tri.v0.normal.xyz * barycentrics.x + tri.v1.normal.xyz * barycentrics.y + tri.v2.normal.xyz * barycentrics.z);
    o.tangent.xyz = normal_mat * normalize(tri.v0.tangent.xyz * barycentrics.x + tri.v1.tangent.xyz * barycentrics.y + tri.v2.tangent.xyz * barycentrics.z);
    o.bitangent.xyz = normal_mat * normalize(tri.v0.bitangent.xyz * barycentrics.x + tri.v1.bitangent.xyz * barycentrics.y + tri.v2.bitangent.xyz * barycentrics.z);

    return o;
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
    if (material.texture_indices0.x == -1)
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
    const Triangle tri = fetch_triangle();
    const Material material = Materials.data[tri.mat_idx];

    p.vertex = interpolated_vertex(tri);

    fetch_albedo(material, p);
    fetch_normal(material, p);
    fetch_roughness(material, p);
    fetch_metallic(material, p);
    fetch_emissive(material, p);

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

    populate_surface_properties(p);

    ray_payload.color = p.albedo.rgb;
}

// ------------------------------------------------------------------------