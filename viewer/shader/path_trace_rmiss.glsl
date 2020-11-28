#include "common.glsl"

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
// Payload ----------------------------------------------------------------
// ------------------------------------------------------------------------

layout(location = 0) rayPayloadInEXT PathTracePayload ray_payload;

// ------------------------------------------------------------------------
// Main -------------------------------------------------------------------
// ------------------------------------------------------------------------

void main()
{
#if defined(RAY_DEBUG_VIEW)
    uint debug_ray_vert_idx = atomicAdd(DebugRayDrawArgs.count, 2);

    DebugRayVertex v0;

    v0.position = vec4(gl_WorldRayOriginEXT, 1.0f);
    v0.color = vec4(ray_payload.color, 1.0f);

    DebugRayVertex v1;

    v1.position = vec4(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT  * gl_RayTmaxEXT, 1.0f);
    v1.color = vec4(ray_payload.color, 1.0f);

    DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 0] = v0;
    DebugRayVertexBuffer.vertices[debug_ray_vert_idx + 1] = v1;
#else
    ray_payload.color = vec3(0.77f, 0.77f, 0.9f) * ray_payload.attenuation;
    ray_payload.hit_distance = 0.0f;
#endif
}

// ------------------------------------------------------------------------