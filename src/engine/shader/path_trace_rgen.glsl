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
    mat4 view_proj_inverse;
    vec4 camera_pos;
    vec4 up_direction;
    vec4 right_direction;
    vec4 focal_plane;
    ivec4 ray_debug_pixel_coord;
    uvec4 launch_id_size;
    float accumulation;
    uint num_lights;
    uint num_frames;
    uint debug_vis;
    uint max_ray_bounces;
    float shadow_ray_bias;
    float focal_length;
    float aperture_radius;
} u_PathTraceConsts;

// ------------------------------------------------------------------------
// Output Payload ---------------------------------------------------------
// ------------------------------------------------------------------------

layout(location = 0) rayPayloadEXT PathTracePayload p_PathTracePayload;

// ------------------------------------------------------------------------
// Structures -------------------------------------------------------------
// ------------------------------------------------------------------------

struct Ray
{
    vec3 origin;
    vec3 direction;
};

// ------------------------------------------------------------------------
// Functions --------------------------------------------------------------
// ------------------------------------------------------------------------

Ray generate_ray(in uvec2 launch_id, in uvec2 launch_size)
{
    Ray ray;

    // Compute Pixel Coordinates
#if defined(RAY_DEBUG_VIEW)
    const vec2 pixel_coord = vec2(u_PathTraceConsts.ray_debug_pixel_coord.xy) + vec2(0.5);
#else
    const vec2 pixel_coord = vec2(launch_id) + vec2(0.5);
#endif
    const vec2 jittered_coord = pixel_coord + vec2(next_float(p_PathTracePayload.rng), next_float(p_PathTracePayload.rng)); 
#if defined(RAY_DEBUG_VIEW)
    const vec2 tex_coord = jittered_coord / vec2(u_PathTraceConsts.ray_debug_pixel_coord.zw);
#else
    const vec2 tex_coord = jittered_coord / vec2(launch_size);
#endif
    
    vec2 tex_coord_neg_to_pos = tex_coord * 2.0 - 1.0;
    // Compute Ray Origin and Direction
    ray.origin = u_PathTraceConsts.camera_pos.xyz;
    vec4 target =  u_PathTraceConsts.view_proj_inverse * vec4(tex_coord_neg_to_pos, 0.0f, 1.0f);
    target /= target.w;
    ray.direction = normalize(target.xyz - ray.origin);

    // Aperture Offset
    float angle = next_float(p_PathTracePayload.rng) * 2.0f * M_PI;
    float radius = sqrt(next_float(p_PathTracePayload.rng));
    vec2 offset = vec2(cos(angle), sin(angle)) * radius * u_PathTraceConsts.aperture_radius;
    float aperture_area = M_PI * u_PathTraceConsts.aperture_radius * u_PathTraceConsts.aperture_radius;

    // Aperture Pos
    vec3 aperture_pos = u_PathTraceConsts.camera_pos.xyz + u_PathTraceConsts.right_direction.xyz * offset.x + u_PathTraceConsts.up_direction.xyz * offset.y;

    vec3 rstart = u_PathTraceConsts.camera_pos.xyz;
    vec3 rdir = -normalize(target.xyz - u_PathTraceConsts.camera_pos.xyz);
    float t = -(dot(rstart, u_PathTraceConsts.focal_plane.xyz) + u_PathTraceConsts.focal_plane.w) / dot(rdir, u_PathTraceConsts.focal_plane.xyz);
    vec3 focus_pos = rstart + rdir * t;
    
    ray.origin = aperture_pos;
    ray.direction = normalize(focus_pos - aperture_pos);

    return ray;
}

// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
    const uvec2 launch_id = uvec2(u_PathTraceConsts.launch_id_size.x + gl_LaunchIDEXT.x, u_PathTraceConsts.launch_id_size.y + gl_LaunchIDEXT.y);
    const uvec2 launch_size = u_PathTraceConsts.launch_id_size.zw;

    if (launch_id.x < launch_size.x && launch_id.y < launch_size.y)
    {
        // Init Payload
        p_PathTracePayload.L = vec3(0.0f);
        p_PathTracePayload.T = vec3(1.0);
        p_PathTracePayload.depth = 0;
        p_PathTracePayload.rng = rng_init(launch_id, u_PathTraceConsts.num_frames);

    #if defined(RAY_DEBUG_VIEW)
        p_PathTracePayload.debug_color = vec3(next_float(p_PathTracePayload.rng) * 0.5f + 0.5f, next_float(p_PathTracePayload.rng) * 0.5f + 0.5f, next_float(p_PathTracePayload.rng) * 0.5f + 0.5f);
    #endif

        Ray ray = generate_ray(launch_id, launch_size);

        uint  ray_flags = 0;
        uint  cull_mask = 0xFF;
        float tmin      = 0.001;
        float tmax      = 10000.0;

        // Trace Ray
        traceRayEXT(u_TopLevelAS, 
                    ray_flags, 
                    cull_mask, 
                    PATH_TRACE_CLOSEST_HIT_SHADER_IDX, 
                    0, 
                    PATH_TRACE_MISS_SHADER_IDX, 
                    ray.origin, 
                    tmin, 
                    ray.direction, 
                    tmax, 
                    0);

    #if !defined(RAY_DEBUG_VIEW)
        // Blend current frames' result with the previous frame
        vec3 clamped_color = min(p_PathTracePayload.L, RADIANCE_CLAMP_COLOR);

        if (u_PathTraceConsts.num_frames == 0)
        {
            vec3 final_color = clamped_color;

    #if defined(VISUALIZE_NANS)
            if (is_nan(p_PathTracePayload.L))
                final_color = vec3(1.0, 0.0, 0.0);
    #endif

            imageStore(i_CurrentColor, ivec2(launch_id), vec4(final_color, 1.0));
        }
        else
        {
            vec3 prev_color = imageLoad(i_PreviousColor, ivec2(launch_id)).rgb;

            //vec3 accumulated_color = mix(p_PathTracePayload.color, prev_color, u_PathTraceConsts.accumulation); 
            vec3 accumulated_color = prev_color + (clamped_color - prev_color) / float(u_PathTraceConsts.num_frames);

            vec3 final_color = accumulated_color;

    #if defined(VISUALIZE_NANS)
            if (is_nan(p_PathTracePayload.L))
                final_color = vec3(1.0, 0.0, 0.0);
    #endif

            imageStore(i_CurrentColor, ivec2(launch_id), vec4(final_color, 1.0));
        }
    #endif
    }
}

// ------------------------------------------------------------------------