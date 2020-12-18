#version 460

// ------------------------------------------------------------------
// INPUTS -----------------------------------------------------------
// ------------------------------------------------------------------

layout (location = 0) in vec3 FS_IN_Position;

// ------------------------------------------------------------------
// OUTPUTS ----------------------------------------------------------
// ------------------------------------------------------------------

layout(location = 0) out vec4 FS_OUT_Color;

// ------------------------------------------------------------------
// DESCRIPTOR SETS --------------------------------------------------
// ------------------------------------------------------------------

layout (set = 0, binding = 0) uniform PerFrameUBO 
{
	vec4 A;
    vec4 B;
    vec4 C;
    vec4 D;
    vec4 E;
    vec4 F;
    vec4 G;
    vec4 H;
    vec4 I;
    vec4 Z;
} u_PerFrameUBO;

// ------------------------------------------------------------------
// PUSH CONSTANTS ---------------------------------------------------
// ------------------------------------------------------------------

layout(push_constant) uniform PushConstants
{
    mat4 view_proj;
    vec3 direction;

} u_PushConstants;

// ------------------------------------------------------------------
// FUNCTIONS --------------------------------------------------------
// ------------------------------------------------------------------

vec3 hosek_wilkie(float cos_theta, float gamma, float cos_gamma)
{
	vec3 chi = (1 + cos_gamma * cos_gamma) / pow(1 + u_PerFrameUBO.H.xyz * u_PerFrameUBO.H.xyz - 2 * cos_gamma * u_PerFrameUBO.H.xyz, vec3(1.5));
    return (1 + u_PerFrameUBO.A.xyz * exp(u_PerFrameUBO.B.xyz / (cos_theta + 0.01))) * (u_PerFrameUBO.C.xyz + u_PerFrameUBO.D.xyz * exp(u_PerFrameUBO.E.xyz * gamma) + u_PerFrameUBO.F.xyz * (cos_gamma * cos_gamma) + u_PerFrameUBO.G.xyz * chi + u_PerFrameUBO.I.xyz * sqrt(cos_theta));
}

// ------------------------------------------------------------------

vec3 hosek_wilkie_sky_rgb(vec3 v, vec3 sun_dir)
{
    float cos_theta = clamp(v.y, 0, 1);
	float cos_gamma = clamp(dot(v, sun_dir), 0, 1);
	float gamma_ = acos(cos_gamma);

	vec3 R = u_PerFrameUBO.Z.xyz * hosek_wilkie(cos_theta, gamma_, cos_gamma);
    return R;
}

// ------------------------------------------------------------------
// MAIN -------------------------------------------------------------
// ------------------------------------------------------------------

void main()
{
    vec3 direction = normalize(FS_IN_Position);

    FS_OUT_Color = vec4(hosek_wilkie_sky_rgb(direction, u_PushConstants.direction), 1.0f);
}

// ------------------------------------------------------------------