#include "common.glsl"
#include "sampling.glsl"

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
    uvec4 num_lights; // x: directional lights, y: point lights, z: spot lights, w: area lights  
    ivec4 ray_debug_pixel_coord;
    float accumulation;
    uint num_frames;
} u_PathTraceConsts;

// ------------------------------------------------------------------------
// Payload ----------------------------------------------------------------
// ------------------------------------------------------------------------

layout(location = 0) rayPayloadEXT PathTracePayload ray_payload;

// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

bool is_nan(vec3 c)
{
    return isnan(c.x) || isnan(c.y) || isnan(c.z);
}

// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
    // Init Payload
    ray_payload.color = vec3(0.0f);
    ray_payload.attenuation = vec3(1.0);
    ray_payload.hit_distance = 0.0f;
    ray_payload.depth = 0;
    ray_payload.rng = rng_init(gl_LaunchIDEXT.xy, u_PathTraceConsts.num_frames);

#if defined(RAY_DEBUG_VIEW)
    ray_payload.color = vec3(next_float(ray_payload.rng) * 0.5f + 0.5f, next_float(ray_payload.rng) * 0.5f + 0.5f, next_float(ray_payload.rng) * 0.5f + 0.5f);
#endif

    // Compute Pixel Coordinates
#if defined(RAY_DEBUG_VIEW)
    const vec2 pixel_coord = vec2(u_PathTraceConsts.ray_debug_pixel_coord.xy) + vec2(0.5);
#else
    const vec2 pixel_coord = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
#endif
    const vec2 jittered_coord = pixel_coord + vec2(next_float(ray_payload.rng), next_float(ray_payload.rng)); 
#if defined(RAY_DEBUG_VIEW)
    const vec2 tex_coord = jittered_coord / vec2(u_PathTraceConsts.ray_debug_pixel_coord.zw);
#else
    const vec2 tex_coord = jittered_coord / vec2(gl_LaunchSizeEXT.xy);
#endif
    vec2 tex_coord_neg_to_pos = tex_coord * 2.0 - 1.0;

    // Compute Ray Origin and Direction
    vec4 origin = u_PathTraceConsts.view_inverse * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 target = u_PathTraceConsts.proj_inverse * vec4(tex_coord_neg_to_pos.x, tex_coord_neg_to_pos.y, 1.0, 1.0);
    vec4 direction = u_PathTraceConsts.view_inverse * vec4(normalize(target.xyz), 0.0);

    uint  ray_flags = gl_RayFlagsOpaqueEXT;
    uint  cull_mask = 0xFF;
    float tmin      = 0.001;
    float tmax      = 10000.0;

    // Trace Ray
    traceRayEXT(u_TopLevelAS, 
                ray_flags, 
                cull_mask, 
                PATH_TRACE_RAY_GEN_SHADER_IDX, 
                PATH_TRACE_CLOSEST_HIT_SHADER_IDX, 
                PATH_TRACE_MISS_SHADER_IDX, 
                origin.xyz, 
                tmin, 
                direction.xyz, 
                tmax, 
                0);

#if !defined(RAY_DEBUG_VIEW)
    // Blend current frames' result with the previous frame
    vec3 clamped_color = min(ray_payload.color, RADIANCE_CLAMP_COLOR);

    if (u_PathTraceConsts.num_frames == 0)
    {
        vec3 final_color = clamped_color;

        if (is_nan(final_color))
            final_color = vec3(1.0, 0.0, 0.0);

        imageStore(i_CurrentColor, ivec2(gl_LaunchIDEXT.xy), vec4(final_color, 1.0));
    }
    else
    {
        vec3 prev_color = imageLoad(i_PreviousColor, ivec2(gl_LaunchIDEXT.xy)).rgb;

        //vec3 accumulated_color = mix(ray_payload.color, prev_color, u_PathTraceConsts.accumulation); 
        vec3 accumulated_color = prev_color + (clamped_color - prev_color) / float(u_PathTraceConsts.num_frames);

        vec3 final_color = accumulated_color;

        if (is_nan(final_color))
            final_color = vec3(1.0, 0.0, 0.0);

        imageStore(i_CurrentColor, ivec2(gl_LaunchIDEXT.xy), vec4(final_color, 1.0));
    }
#endif
}

// ------------------------------------------------------------------------