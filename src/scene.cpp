#include "scene.h"
#include "mesh.h"
#include "staging.h"

void create_scene(gpu_context_t& context, std::vector<mesh_data_t>& mesh_data, scene_t& scene)
{
    staging_buffer_t staging;
    init_staging_buffer(staging, &context);

    u32 vbo_size = 0;
    u32 ibo_size = 0;
    for (auto& mesh : mesh_data)
    {
        vbo_size += static_cast<u32>(mesh.vertices.size() * sizeof(vertex_t));
        ibo_size += static_cast<u32>(mesh.indices.size()  * sizeof(u32));
    }

    create_buffer(context, vbo_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, &scene.vbo);
    create_buffer(context, ibo_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, &scene.ibo);
    
    // load mesh data
    u32 vertex_offset = 0;
    u32 index_offset  = 0;
    scene.meshes.resize(mesh_data.size());
    auto pool = context.frames[0].command_pool;
    auto cmd  = begin_one_time_command_buffer(context, pool);
    for (size_t i = 0; i < mesh_data.size(); ++i)
    {
        auto& data = mesh_data[i];
        copy_to_buffer(staging, cmd, scene.vbo, static_cast<u32>(data.vertices.size() * sizeof(vertex_t)), 
                (void*)data.vertices.data(), vertex_offset * sizeof(vertex_t));
        copy_to_buffer(staging, cmd, scene.ibo, static_cast<u32>(data.indices.size() * sizeof(u32)), 
                (void*)data.indices.data(), index_offset * sizeof(u32));
        
        auto& mesh = scene.meshes[i];
        mesh.id = i;
        mesh.index_count = static_cast<u32>(data.indices.size());
        mesh.index_offset = index_offset;
        mesh.vertex_offset = vertex_offset;
        mesh.vbo = scene.vbo.handle;
        mesh.ibo = scene.ibo.handle;
        
        // update offsets
        vertex_offset += static_cast<u32>(data.vertices.size());
        index_offset  += static_cast<u32>(data.indices.size());
    }

    end_and_submit_command_buffer(cmd, context.q_transfer);
    VK_CHECK( vkQueueWaitIdle(context.q_transfer) );
    vkFreeCommandBuffers(context.device, pool, 1, &cmd);

    reset(staging);
    destroy_staging_buffer(staging);
}
