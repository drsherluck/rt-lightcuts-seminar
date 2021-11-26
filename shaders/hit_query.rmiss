#version 460 
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"

layout(location = 0) rayPayloadInEXT query_output_t payload;

void main()
{
    payload.hit = false;
}

