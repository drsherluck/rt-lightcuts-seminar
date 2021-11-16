#ifndef SHADER_DATA_H
#define SHADER_DATA_H

#ifdef GLSL_EXT_64
#define GLSL
#endif

#ifndef GLSL
#include "common.h"
#include "math.h"

#define mat3     m3x3
#define mat4     m4x4
#define vec4     v4
#define vec3     v3
#define uint     u32
#define int      i32
#define float    f32
#define uint64_t u64
#endif

struct light_t
{
#ifdef GLSL
    vec3 pos;
    vec3 color;
#else 
    // 16 byte alignment
    aligned_v3 pos;
    aligned_v3 color;
#endif
};

struct material_t
{
    vec3  base_color;
    float roughness;
    float metalness;
    float emissive;
#ifndef GLSL
    f32 _pad[2];
#endif
};

struct model_t
{
    int  material_index;
#ifndef GLSL
    f32 _pad[3]; 
#endif 
    mat4 m_model;
    mat4 m_normal_model; 
};

struct camera_ubo_t 
{
#ifndef GLSL
    aligned_v3 pos;
#else
    vec3 pos;
#endif
    mat4 view;
    mat4 proj;
    mat4 inv_view;
    mat4 inv_proj;
};

struct encoded_t
{
    uint code;
    uint id;
};

struct node_t
{
    vec3  bbox_min;
    float intensity;
    vec3  bbox_max;
    uint  id;
};

struct mesh_info_t
{
    int  material_index;
    uint index_offset;
    uint vertex_offset;
};

#if defined GLSL_EXT_64 || !defined GLSL
struct scene_info_t
{
    uint64_t vertex_address;
    uint64_t index_address;
};
#endif

#ifndef GLSL
#undef mat3
#undef mat4 
#undef vec4 
#undef vec3 
#undef uint
#undef int  
#undef float 
#undef uint64_t 
#endif 

#endif // SHADER_DATA_H
