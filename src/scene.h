#ifndef SCENE_H
#define SCENE_H

#include "math.h"
#include "backend.h"
#include "acceleration_struct.h"

#include <vector>

struct mesh_data_t;
struct material_t;

struct entity_t
{
    m4x4 m_model;
    i32 material_index;
    u32  mesh_id;
};

struct mesh_t
{
    u32      id;
    u32      index_count;
    u32      index_offset;
    u32      vertex_count;
    u32      vertex_offset;

    // todo: remove these
    VkBuffer vbo;
    VkBuffer ibo;
};

struct prefab_t
{
    u32 mesh_id;
    i32 material_index;
};

struct light_t
{
    aligned_v3 position;
    aligned_v3 color;
};

struct model_t
{
    i32  material_index;
    f32 _p[3];
    m4x4 m_model;
    m4x4 m_normal_model; 
};

struct camera_ubo_t 
{
    aligned_v3 position;
    m4x4 view;
    m4x4 proj;
    m4x4 inv_view;
    m4x4 inv_proj;
};

struct scene_t
{
    std::vector<entity_t> entities;
    std::vector<light_t>  lights;

    buffer_t vbo;
    buffer_t ibo;
    std::vector<mesh_t>                   meshes;
    std::vector<material_t>               materials;
    acceleration_structure_t              tlas;
    std::vector<acceleration_structure_t> blas; // same order as meshes
};

inline void add_entity(scene_t& scene, u32 mesh_id, i32 material_index, m4x4 m_model)
{
    scene.entities.emplace_back(entity_t{m_model, material_index, mesh_id});
}

inline void add_light(scene_t& scene, v3 pos, v3 color)
{
    scene.lights.emplace_back(light_t{vec4(pos, 1), vec4(color, 1)});
}

inline void add_material(scene_t& scene, material_t& material)
{
    scene.materials.push_back(material);
}

// creates vbo and ibo and uploads the data to the gpu
void create_scene(gpu_context_t& context, std::vector<mesh_data_t>& mesh_data, scene_t& scene);
void destroy_scene(gpu_context_t& context, scene_t& scene);
void create_acceleration_structures(gpu_context_t& context, scene_t& scene);
void update_acceleration_structures(gpu_context_t& context, scene_t& scene);

#endif
