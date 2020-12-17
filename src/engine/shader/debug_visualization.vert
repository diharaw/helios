#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.glsl"

// ------------------------------------------------------------------------
// Inputs -----------------------------------------------------------------
// ------------------------------------------------------------------------

layout (location = 0) in vec4 VS_IN_Position;
layout (location = 1) in vec4 VS_IN_TexCoord;
layout (location = 2) in vec4 VS_IN_Normal;
layout (location = 3) in vec4 VS_IN_Tangent;
layout (location = 4) in vec4 VS_IN_Bitangent;

// ------------------------------------------------------------------------
// Outputs ----------------------------------------------------------------
// ------------------------------------------------------------------------

layout (location = 0) out vec2 FS_IN_TexCoord;
layout (location = 1) out vec3 FS_IN_Normal;
layout (location = 2) out vec3 FS_IN_Tangent;
layout (location = 3) out vec3 FS_IN_Bitangent;

// ------------------------------------------------------------------------
// Set 0 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 0, binding = 1, std430) readonly buffer InstanceBuffer 
{
    Instance data[];
} Instances;

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
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
    const Instance instance = Instances.data[u_PushConstants.instance_id];

    FS_IN_TexCoord = VS_IN_TexCoord.xy;

    mat3 normal_mat = mat3(instance.normal_matrix);

    FS_IN_Normal = normalize(normal_mat * VS_IN_Normal.xyz);
    FS_IN_Tangent = normalize(normal_mat * VS_IN_Tangent.xyz);
    FS_IN_Bitangent = normalize(normal_mat * VS_IN_Bitangent.xyz);

    gl_Position = u_PushConstants.view_proj * instance.model_matrix * vec4(VS_IN_Position.xyz, 1.0f);
}

// ------------------------------------------------------------------------