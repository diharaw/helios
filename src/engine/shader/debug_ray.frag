#version 460

layout (location = 0) in vec4 FS_IN_Color;

layout(location = 0) out vec4 FS_OUT_Color;

void main()
{
    FS_OUT_Color = FS_IN_Color;
}
