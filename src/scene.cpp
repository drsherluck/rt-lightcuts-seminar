#include "scene.h"
#include "mesh.h"
#include "staging.h"

#include <cstring>

static VkTransformMatrixKHR to_transform_matrix_khr(m4x4 model)
{
    m4x4 t_model = transpose(model);
    VkTransformMatrixKHR out;
    memcpy(&out, &t_model, sizeof(VkTransformMatrixKHR));
    return out;
}

void create_scene(gpu_context_t& context, std::vector<mesh_data_t>& mesh_data, scene_t& scene)
{
    staging_buffer_t staging(&context);

    u32 vbo_size = 0;
    u32 ibo_size = 0;
    for (auto& mesh : mesh_data)
    {
        vbo_size += static_cast<u32>(mesh.vertices.size() * sizeof(vertex_t));
        ibo_size += static_cast<u32>(mesh.indices.size()  * sizeof(u32));
    }

    create_buffer(context, vbo_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, &scene.vbo);
    create_buffer(context, ibo_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, &scene.ibo);
    
    // load mesh data
    u32 vertex_offset = 0;
    u32 index_offset  = 0;
    scene.meshes.resize(mesh_data.size());
    begin_upload(staging);
    for (size_t i = 0; i < mesh_data.size(); ++i)
    {
        auto& data = mesh_data[i];
        copy_to_buffer(staging, scene.vbo, static_cast<u32>(data.vertices.size() * sizeof(vertex_t)), 
                (void*)data.vertices.data(), vertex_offset * sizeof(vertex_t));
        copy_to_buffer(staging, scene.ibo, static_cast<u32>(data.indices.size() * sizeof(u32)), 
                (void*)data.indices.data(), index_offset * sizeof(u32));
        
        auto& mesh = scene.meshes[i];
        mesh.id = i;
        mesh.index_count = static_cast<u32>(data.indices.size());
        mesh.index_offset = index_offset;
        mesh.vertex_count = static_cast<u32>(data.vertices.size());
        mesh.vertex_offset = vertex_offset;
        mesh.vbo = scene.vbo.handle;
        mesh.ibo = scene.ibo.handle;
        
        // update offsets
        vertex_offset += static_cast<u32>(data.vertices.size());
        index_offset  += static_cast<u32>(data.indices.size());
    }

    end_upload(staging);
    VK_CHECK( vkQueueWaitIdle(context.q_transfer) );
}

void create_acceleration_structures(gpu_context_t& context, scene_t& scene)
{
    if (scene.meshes.size() == 0)
    {
        LOG_ERROR("No meshes loaded, skipping accelerations structure build");
    }
    else
    {
        auto vertex_address = get_buffer_device_address(context, scene.vbo.handle);
        auto index_address  = get_buffer_device_address(context, scene.ibo.handle);    

        auto& builder = scene.as_builder;
        init_acceleration_structure_builder(builder, &context);

        // create blases
        for (size_t i = 0; i < scene.meshes.size(); ++i)
        {
            // should be already sorted by id 
            auto& mesh = scene.meshes[i];

            VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexStride = sizeof(vertex_t);
            triangles.vertexData.deviceAddress = vertex_address;
            triangles.maxVertex = mesh.vertex_count;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress = index_address;

            VkAccelerationStructureGeometryKHR geom = {};
            geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geom.pNext = nullptr;
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geom.geometry.triangles = triangles;
            geom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
            
            VkAccelerationStructureBuildRangeInfoKHR offset;
            offset.primitiveCount = mesh.index_count / 3;
            offset.primitiveOffset = mesh.index_offset * sizeof(u32);
            offset.firstVertex = mesh.vertex_offset;
            offset.transformOffset = 0;

            LOG_INFO("PRIMITIVE OFFSET = %d", offset.primitiveOffset);
            LOG_INFO("FIRST VERTEX = %d", offset.firstVertex);
            builder.inputs.push_back({geom, offset});
        }
        build_blases(builder, scene.blas);
        
        // create tlas
        scene.instances.resize(scene.entities.size(), {});
        for (size_t i = 0; i < scene.entities.size(); ++i)
        {
            auto& entity = scene.entities[i];
            VkAccelerationStructureDeviceAddressInfoKHR address_info = {};
            address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            address_info.accelerationStructure = scene.blas[entity.mesh_id].handle;
            const VkDeviceAddress address = vkGetAccelerationStructureDeviceAddress(context.device, &address_info);

            VkAccelerationStructureInstanceKHR& instance = scene.instances[i];
            instance.transform = to_transform_matrix_khr(scene.entities[i].m_model);
            instance.instanceCustomIndex = i; // todo: should sort entities first by mesh id or give them a unique id
            instance.accelerationStructureReference = address; 
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0; // todo: for different shader use (materials etc..)
        }
        build_tlas(builder, scene.instances, scene.tlas);

        LOG_INFO("Acceleration structures built");
    }
}

void update_acceleration_structures(gpu_context_t& context, scene_t& scene)
{
    auto& builder = scene.as_builder;
    for (size_t i = 0; i < scene.entities.size(); ++i)
    {
        scene.instances[i].transform = to_transform_matrix_khr(scene.entities[i].m_model);
    }
    build_tlas(builder, scene.instances, scene.tlas, true);
}

void destroy_scene(gpu_context_t& context, scene_t& scene)
{
    destroy_acceleration_structure_builder(scene.as_builder);
    vkDestroyAccelerationStructure(context.device, scene.tlas.handle, nullptr);
    destroy_buffer(context, scene.tlas.buffer);
    for (auto& b : scene.blas)
    {
        vkDestroyAccelerationStructure(context.device, b.handle, nullptr);
    }
    destroy_buffer(context, scene.blas[0].buffer);
    destroy_buffer(context, scene.vbo);
    destroy_buffer(context, scene.ibo);
}


