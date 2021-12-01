#version 460 
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "rtx.inc"

layout(location = 0) rayPayloadInEXT payload_t payload;

void main()
{
    payload.color = vec4(0.0, 0.0, 0.0, 1.0);
    payload.hit = false;
}

