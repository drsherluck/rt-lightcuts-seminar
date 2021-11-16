#version 460
#extension GL_GOOGLE_include_directive : enable

#define GLSL
#include "../src/shader_data.h"

struct model_data
{
    int mesh_index;
    mat4 model;
    mat4 normal; 
};

layout(std140, set = 0, binding = 2) readonly buffer model_sbo 
{
    model_data models[];
};

layout(std140, set = 0, binding = 0) uniform camera_ubo 
{
    camera_ubo_t camera;
};

layout(push_constant) uniform constants
{
    mat4 projection;
    mat4 view;
    int num_lights;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out VS_OUT
{
    vec3 frag_pos;
    vec3 frag_normal;
    vec2 texcoord;
    flat int mesh_index;
    flat int light_count;
};

void main() 
{
    model_data data = models[gl_InstanceIndex]; 

    mat4 model_view = camera.view * data.model;
    vec4 pos = model_view * vec4(position, 1);
    gl_Position = camera.proj * pos;

	frag_pos = pos.xyz;
	frag_normal = normalize(mat3(data.normal) * normal);
	texcoord = uv;
    mesh_index = data.mesh_index;
    light_count = num_lights;
}
