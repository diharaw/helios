#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.glsl"

#define DEBUG_VISUALIZATION_ALBEDO 0
#define DEBUG_VISUALIZATION_NORMAL 1
#define DEBUG_VISUALIZATION_ROUGHNESS 2
#define DEBUG_VISUALIZATION_METALLIC 3
#define DEBUG_VISUALIZATION_EMISSIVE 4

// ------------------------------------------------------------------------
// Inputs -----------------------------------------------------------------
// ------------------------------------------------------------------------

layout (location = 0) in vec2 FS_IN_TexCoord;
layout (location = 1) in vec3 FS_IN_Normal;
layout (location = 2) in vec3 FS_IN_Tangent;
layout (location = 3) in vec3 FS_IN_Bitangent;

// ------------------------------------------------------------------------
// Outputs ----------------------------------------------------------------
// ------------------------------------------------------------------------

layout(location = 0) out vec4 FS_OUT_Color;

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

// ------------------------------------------------------------------------
// Set 1 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 1, binding = 0) readonly buffer SubmeshInfoBuffer 
{
    uvec2 data[];
} SubmeshInfo[];

// ------------------------------------------------------------------------
// Set 2 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 2, binding = 0) uniform sampler2D s_Textures[];

// ------------------------------------------------------------------------
// Push Constants ---------------------------------------------------------
// ------------------------------------------------------------------------

layout(push_constant) uniform PushConstants
{
    mat4 view_proj;
    uint instance_id;
    uint submesh_id;
    uint debug_visualization;
} u_PushConstants;

// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

vec3 get_normal_from_map(vec3 tangent, vec3 bitangent, vec3 normal, vec2 tex_coord, uint normal_map_idx)
{
    // Create TBN matrix.
    mat3 TBN = mat3(normalize(tangent), normalize(bitangent), normalize(normal));

    // Sample tangent space normal vector from normal map and remap it from [0, 1] to [-1, 1] range.
    vec3 n = normalize(texture(s_Textures[nonuniformEXT(normal_map_idx)], tex_coord).rgb * 2.0 - 1.0);

    // Multiple vector by the TBN matrix to transform the normal from tangent space to world space.
    n = normalize(TBN * n);

    return n;
}

// ------------------------------------------------------------------------

vec3 fetch_albedo(in Material material, in vec2 texcoord)
{
    if (material.texture_indices0.x == -1)
        return material.albedo.rgb;
    else
        return texture(s_Textures[nonuniformEXT(material.texture_indices0.x)], texcoord).rgb;
}

// ------------------------------------------------------------------------

vec3 fetch_normal(in Material material, in vec3 tangent, in vec3 bitangent, in vec3 normal, in vec2 texcoord)
{
    if (material.texture_indices0.y == -1)
        return normal;
    else
        return get_normal_from_map(tangent, bitangent, normal, texcoord, material.texture_indices0.y);
}

// ------------------------------------------------------------------------

float fetch_roughness(in Material material, in vec2 texcoord)
{
    if (material.texture_indices0.z == -1)
        return material.roughness_metallic.r;
    else
        return texture(s_Textures[nonuniformEXT(material.texture_indices0.z)], texcoord)[material.texture_indices1.z];
}

// ------------------------------------------------------------------------

float fetch_metallic(in Material material, in vec2 texcoord)
{
    if (material.texture_indices0.w == -1)
        return material.roughness_metallic.g;
    else
        return texture(s_Textures[nonuniformEXT(material.texture_indices0.w)], texcoord)[material.texture_indices1.w];
}

// ------------------------------------------------------------------------

vec3 fetch_emissive(in Material material, in vec2 texcoord)
{
    if (material.texture_indices1.x == -1)
        return material.emissive.rgb;
    else
        return texture(s_Textures[nonuniformEXT(material.texture_indices1.x)], texcoord).rgb;
}

// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
    const Instance instance = Instances.data[u_PushConstants.instance_id];
    const uint material_id = SubmeshInfo[u_PushConstants.instance_id].data[u_PushConstants.submesh_id].y;
    const Material material = Materials.data[material_id];

    vec4 color = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    if (u_PushConstants.debug_visualization == DEBUG_VISUALIZATION_ALBEDO)
        color.xyz = fetch_albedo(material, FS_IN_TexCoord);
    else if (u_PushConstants.debug_visualization == DEBUG_VISUALIZATION_NORMAL)
        color.xyz = fetch_normal(material, FS_IN_Tangent, FS_IN_Bitangent, FS_IN_Normal, FS_IN_TexCoord);
    else if (u_PushConstants.debug_visualization == DEBUG_VISUALIZATION_ROUGHNESS) 
        color.xyz = vec3(fetch_roughness(material, FS_IN_TexCoord));
    else if (u_PushConstants.debug_visualization == DEBUG_VISUALIZATION_METALLIC) 
        color.xyz = vec3(fetch_metallic(material, FS_IN_TexCoord));
    else if (u_PushConstants.debug_visualization == DEBUG_VISUALIZATION_EMISSIVE) 
        color.xyz = fetch_emissive(material, FS_IN_TexCoord);

    FS_OUT_Color = color;
}

// ------------------------------------------------------------------------