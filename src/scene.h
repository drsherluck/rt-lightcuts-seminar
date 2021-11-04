#ifndef SCENE_H
#define SCENE_H

#include "math.h"
#include "backend.h"

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
    u32      vertex_offset;
    VkBuffer vbo;
    VkBuffer ibo;
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
    aligned_v3 direction;
};

struct scene_t
{
    m4x4 m_transform;
    std::vector<entity_t> entities;
    std::vector<light_t>  lights;
    buffer_t vbo, ibo;

    std::vector<mesh_t>   meshes;
    std::vector<material_t> materials;

    scene_t() = default;
    scene_t(m4x4 transform)
    {
        m_transform = transform;
    }
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

#endif
