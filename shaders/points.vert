#version 460
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"

layout(std140, set = 0, binding = 0) uniform camera_ubo 
{
    camera_ubo_t camera;
};

// light data
layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 frag_color;

void main()
{
    gl_Position = camera.proj * camera.view * vec4(pos.xyz, 1);
    gl_PointSize = 10;
    frag_color = vec4(color.xyz, 1);
}
