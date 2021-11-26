#version 460 
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : enable

#define GLSL_EXT_64
#include "../src/shader_data.h"

struct vertex_t
{
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct model_data
{
    int mesh_index;
    mat4 model;
    mat4 normal;
};

layout(set = 0, binding = 1) readonly buffer lights_buffer
{
    light_t lights[]; 
};

layout(set = 0, binding = 2) readonly buffer model_sbo
{
    model_data models[];
};

layout(set = 0, binding = 3) readonly buffer material_sbo
{
    material_t materials[];
};

layout(set = 0, binding = 4) readonly buffer mesh_buffer
{
    mesh_info_t meshes[];
};

layout(buffer_reference, scalar) readonly buffer vertex_buffer 
{
    vertex_t vertices[];
};

layout(buffer_reference, scalar) readonly buffer index_buffer 
{
    uint indices[];
};

layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 1, binding = 1) uniform scene_ubo 
{
    scene_info_t scene;
};

struct payload_t 
{
    vec3 hit_pos;
    bool hit;
};

layout(location = 0) rayPayloadInEXT payload_t payload;
hitAttributeEXT vec2 attribs;

void main()
{
    vertex_buffer vbo = vertex_buffer(scene.vertex_address);
    index_buffer  ibo = index_buffer(scene.index_address);

    model_data md = models[gl_InstanceCustomIndexEXT];
    mesh_info_t info = meshes[md.mesh_index];
    const uint idx = uint(info.index_offset) + 3 * gl_PrimitiveID;
    uvec3 triangle = uvec3(ibo.indices[idx + 0], ibo.indices[idx + 1], ibo.indices[idx + 2]) + uvec3(info.vertex_offset);
    const vec3 barycenter = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    const vertex_t v0 = vbo.vertices[triangle.x];
    const vertex_t v1 = vbo.vertices[triangle.y];
    const vertex_t v2 = vbo.vertices[triangle.z];

    vec3 position       = v0.pos * barycenter.x + v1.pos * barycenter.y + v2.pos * barycenter.z;
    vec3 world_position = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    
    payload.hit_pos = world_position;
    payload.hit     = true;
}
