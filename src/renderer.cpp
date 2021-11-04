#include "renderer.h"
#include "descriptor.h"
#include "scene.h"
#include "camera.h"
#include "mesh.h" // material struct 

#include <algorithm>
#include <cassert>

#define MAX_ENTITIES 100
#define MAX_LIGHTS 100

renderer_t::renderer_t(window_t* _window) : window(_window)
{
    init_context(context, window);
    init_staging_buffer(staging, &context);

    //create_render_pass(context, &render_pass);
    // create descriptor sets
    descriptor_allocator_t descriptor_allocator;
    init_descriptor_allocator(context.device, 10, &descriptor_allocator);
    
    descriptor_set_layout_t layout_set0;
    add_binding(layout_set0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    add_binding(layout_set0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    add_binding(layout_set0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    add_binding(layout_set0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    build_descriptor_set_layout(context.device, layout_set0);
    
    frame_resources.resize(context.frames.size());
    for (size_t i = 0; i < context.frames.size(); ++i)
    {

        create_buffer(context, sizeof(camera_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_camera);
        create_buffer(context, MAX_LIGHTS * sizeof(light_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].ubo_light);
        create_buffer(context, MAX_ENTITIES * sizeof(model_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].ubo_model); 
        create_buffer(context, MAX_ENTITIES * sizeof(material_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].sbo_material);

        VkDescriptorBufferInfo ubo_camera_info = { frame_resources[i].ubo_camera.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_light_info  = { frame_resources[i].ubo_light.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_model_info  = { frame_resources[i].ubo_model.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo sbo_material_info = { frame_resources[i].sbo_material.handle, 0, VK_WHOLE_SIZE };

        descriptor_set_t set0(layout_set0);
        bind_buffer(set0, 0, &ubo_camera_info);
        bind_buffer(set0, 1, &ubo_light_info);
        bind_buffer(set0, 2, &ubo_model_info);
        bind_buffer(set0, 3, &sbo_material_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set0));
    }

    pipeline_description_t pipeline_description;
    init_graphics_pipeline_description(context, pipeline_description);
    add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT,   "main", "shaders/pbr.vert.spv");
    add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/pbr.frag.spv");
    graphics_pipeline.descriptor_set_layouts.push_back(layout_set0.handle);
    build_graphics_pipeline(context, pipeline_description, graphics_pipeline);

    // create rt pipeline
    // create compute pipeline
}

renderer_t::~renderer_t()
{
    destroy_staging_buffer(staging);
    destroy_context(context);
}

struct batch_t 
{
    u32 instance_count;
    u32 mesh_id;
};

struct push_constant_t
{
    m4x4 projection;
    m4x4 view;
    i32  num_lights;
};

void renderer_t::draw_scene(scene_t& scene, camera_t& camera)
{
    static bool mat_uploaded[2] = {false, false};

    i32 frame_index;
    frame_t *frame;
    prepare_frame(context, &frame_index, &frame);
    get_next_swapchain_image(context, frame);
    
    {
        std::sort(std::begin(scene.entities), std::end(scene.entities),
                [](const entity_t& a, const entity_t& b) 
                { return a.mesh_id < b.mesh_id; });

        auto pool = frame->command_pool;
        VK_CHECK( vkResetCommandPool(context.device, pool, 0) );
        auto upload_cmd  = begin_one_time_command_buffer(context, pool);

        if (!mat_uploaded[frame_index])
        {
            copy_to_buffer(staging, upload_cmd, frame_resources[frame_index].sbo_material, scene.materials.size() * sizeof(material_t), (void*)scene.materials.data(), 0);
            mat_uploaded[frame_index] = true;
        }

        // todo change to one copy later
        u64 offset_light = 0;
        for (light_t light : scene.lights)
        {
            light.position = camera.m_view * light.position;
            copy_to_buffer(staging, upload_cmd, frame_resources[frame_index].ubo_light, sizeof(light_t), (void*)&light, offset_light);
            offset_light += sizeof(light_t);
        }
        
        camera_ubo_t camera_data;
        camera_data.position.xyz = camera.position; 
        camera_data.direction.xyz = camera.get_direction();
        copy_to_buffer(staging, upload_cmd, frame_resources[frame_index].ubo_camera, sizeof(camera_ubo_t), (void*)&camera_data, 0);

        auto& meshes = scene.meshes;
        std::vector<batch_t> batches(meshes.size(), batch_t{0,0});
        std::vector<model_t> model_data(scene.entities.size());
        for (size_t i = 0; i < scene.entities.size(); ++i)
        {   
            const auto& entity = scene.entities[i];
            auto& batch = batches[entity.mesh_id];
            batch.mesh_id = entity.mesh_id;
            batch.instance_count++;
    
            auto& data = model_data[i];
            data.m_model = entity.m_model;
            data.m_normal_model = inverse_transpose(camera.m_view * entity.m_model);
            data.material_index = entity.material_index;
        }
        copy_to_buffer(staging, upload_cmd, frame_resources[frame_index].ubo_model, model_data.size() * sizeof(model_t), (void*)model_data.data());

        end_and_submit_command_buffer(upload_cmd, context.q_transfer);
        VK_CHECK( wait_for_queue(context.q_transfer) );
        vkFreeCommandBuffers(context.device, pool, 1, &upload_cmd);
        reset(staging);
    
        // begin recording commands
        VkCommandBuffer cmd = frame->command_buffer;
        VK_CHECK( begin_command_buffer(cmd) );

        VkClearValue clear[2];
        clear[0].color = {0, 0, 0, 0};
        clear[1].depthStencil = {1, 0};
        begin_render_pass(context, cmd, clear, 2);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline.handle);

        // bind entity descriptors
        u32 first_instance_id = 0;
        VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &scene.vbo.handle, offsets);
        vkCmdBindIndexBuffer(cmd, scene.ibo.handle, 0, VK_INDEX_TYPE_UINT32);
        u32 descriptor_count = static_cast<u32>(frame_resources[frame_index].descriptor_sets.size());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline.layout, 0, descriptor_count, 
                frame_resources[frame_index].descriptor_sets.data(), 0, nullptr);

        // camera matrices
        push_constant_t constant;
        constant.projection = camera.m_proj;
        constant.view       = camera.m_view;
        constant.num_lights = static_cast<i32>(scene.lights.size());
        vkCmdPushConstants(cmd, graphics_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant_t), &constant);

        for (const auto& batch : batches)
        {
            auto& mesh = meshes[batch.mesh_id];
            vkCmdDrawIndexed(cmd, mesh.index_count, batch.instance_count, 
                    mesh.index_offset, mesh.vertex_offset, first_instance_id);
            first_instance_id += batch.instance_count;
        }
        vkCmdEndRenderPass(cmd);
        VK_CHECK( vkEndCommandBuffer(cmd) );
    }

    graphics_submit_frame(context, frame);
    present_frame(context, frame);
}


