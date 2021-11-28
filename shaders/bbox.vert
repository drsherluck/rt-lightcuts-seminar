#version 460
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"

layout(std140, set = 0, binding = 0) uniform camera_ubo 
{
    camera_ubo_t camera;
};

layout(std430, set = 1, binding = 0) readonly buffer sbo_cut
{
    bool should_highlight[];
};

layout(push_constant) uniform constants 
{
    vec3 color;
    bool is_bbox;
    vec3 highlight;
    uint offset;
    bool only_selected;
};

layout(location = 0) in  vec4 pos;
layout(location = 0) out vec3 frag_color;
void main()
{
    gl_Position = camera.proj * camera.view * vec4(pos.xyz, 1);
    uint i = uint(gl_VertexIndex/24) + offset;
    // discard if should not display
    if (is_bbox && only_selected && !should_highlight[i])
    {
        gl_Position = vec4(0); // not displayed 
    }

    if (is_bbox && should_highlight[i])
    {
        frag_color = highlight;
    }
    else
    {
        frag_color = color; 
    } 
}
