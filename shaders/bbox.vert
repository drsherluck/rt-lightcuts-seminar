#version 460
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"

layout(std140, set = 0, binding = 0) uniform camera_ubo 
{
    camera_ubo_t camera;
};

layout(location = 0) in vec4 pos;

void main()
{
    gl_Position = camera.proj * camera.view * vec4(pos.xyz, 1);
}
