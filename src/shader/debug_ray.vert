#version 460

layout (location = 0) in vec4 VS_IN_Position;
layout (location = 1) in vec4 VS_IN_Color;

layout(location = 0) out vec4 FS_IN_Color;

layout(push_constant) uniform PathTraceConsts
{
    mat4 view_proj;
} u_PathTraceConsts;

void main()
{
    FS_IN_Color = VS_IN_Color;
    gl_Position = u_PathTraceConsts.view_proj * VS_IN_Position;
}
