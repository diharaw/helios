#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#define DIRECT_LIGHTING_INTEGRATOR
#define RAY_DEBUG_VIEW

#include "path_trace_rchit.glsl"