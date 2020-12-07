#include "common.glsl"

// ------------------------------------------------------------------------
// Set 0 ------------------------------------------------------------------
// ------------------------------------------------------------------------

layout (set = 0, binding = 4) uniform samplerCube s_EnvironmentMap;

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
#endif

// ------------------------------------------------------------------------
// Input Payload ----------------------------------------------------------
// ------------------------------------------------------------------------

rayPayloadInEXT PathTracePayload p_PathTracePayload;

// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
#if defined(RAY_DEBUG_VIEW)
    // Skip the primary ray
    if (p_PathTracePayload.depth > 0)
    {
        uint debug_ray_vert_idx = atomicAdd(DebugRayDrawArgs.count, 2);

        DebugRayVertex v0;

        v0.position = vec4(gl_WorldRayOriginEXT, 1.0f);
        v0.color = vec4(p_PathTracePayload.debug_color, 1.0f);

        DebugRayVertex v1;

        v1.position = vec4(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT  * gl_RayTmaxEXT, 1.0f);
        v1.color = vec4(p_PathTracePayload.debug_color, 1.0f);

        DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 0] = v0;
        DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 1] = v1;
    }
#else
    vec3 environment_map_sample = texture(s_EnvironmentMap, gl_WorldRayDirectionEXT).rgb; 

    if (p_PathTracePayload.depth == 0)
        p_PathTracePayload.L = environment_map_sample;
    else
        p_PathTracePayload.L = p_PathTracePayload.T * environment_map_sample;
#endif
}

// ------------------------------------------------------------------------