#version 450

#define TONE_MAP_OPERATOR_ACES 0
#define TONE_MAP_OPERATOR_REINHARD 1

layout(set = 0, binding = 0) uniform sampler2D samplerColor;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    float exposure;
    uint tone_map_operator;
} u_PushConstants;

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 aces_film(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}

vec3 reinhard(vec3 x)
{
    return (x / (1.0 + x));
}

void main()
{
    vec3 color = texture(samplerColor, inUV).rgb;

    // Apply exposure
    color *= u_PushConstants.exposure;

    // Tone mapping
    if (u_PushConstants.tone_map_operator == TONE_MAP_OPERATOR_ACES)
        color = aces_film(color);
    else if (u_PushConstants.tone_map_operator == TONE_MAP_OPERATOR_REINHARD)
        color = reinhard(color);

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    outFragColor = vec4(color, 1.0);
}