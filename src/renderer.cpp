#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "mesh.h" // material struct 
#include "debug_util.h"

#include <cassert>

#define ENABLE_MORTON_ENCODE 1
#define ENABLE_SORT_LIGHTS 0
#define ENABLE_LIGHT_TREE 1
#define ENABLE_RTX 1

#define MAX_DESCRIPTOR_SETS 50
#define MAX_ENTITIES 100
#define MAX_TREE_HEIGTH 20
#define MAX_LIGHTS (1 << 20)
#define MAX_LIGHT_TREE_SIZE (1 << 21)

struct mesh_metadata_t
{
    i32 material_index;
    u32 index_offset;
    u32 vertex_offset;
};

struct scene_info_t
{
    u64 vertex_address;
    u64 index_address;
};

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

struct encoded_light_t 
{
    u32 morton_code;
    u32 index;
};

struct node_t
{
    v3 aabb_min; // 16 
    f32 intensity;
    v3 aabb_max; // 16
    u32 id;
};

renderer_t::renderer_t(window_t* _window) : window(_window)
{
    init_context(context, window);
    //staging = staging_buffer_t(&context);
    init_staging_buffer(staging, &context);
    init_descriptor_allocator(context.device, MAX_DESCRIPTOR_SETS,  &descriptor_allocator);

    //create_render_pass(context, &render_pass);
    
    // create descriptor layouts for pipelines
    // set 0 
    set_layouts.resize(6);
    auto& layout_set0 = set_layouts[0];
    add_binding(layout_set0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    add_binding(layout_set0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT   | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    build_descriptor_set_layout(context.device, layout_set0);
    // set 1
    auto& layout_set1 = set_layouts[1];
    add_binding(layout_set1, 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set1, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    build_descriptor_set_layout(context.device, layout_set1);
    // set 2
    auto& layout_set2 = set_layouts[2];
    add_binding(layout_set2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    build_descriptor_set_layout(context.device, layout_set2);
    // set 3 (morton_encode)
    auto& layout_set3 = set_layouts[3];
    add_binding(layout_set3, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    add_binding(layout_set3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    build_descriptor_set_layout(context.device, layout_set3);
    // set 4 (sort)
    auto& layout_set4 = set_layouts[4];
    add_binding(layout_set4, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    build_descriptor_set_layout(context.device, layout_set4);
    // set 5 (tree builder)
    auto& layout_set5 = set_layouts[5];
    add_binding(layout_set5, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    add_binding(layout_set5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    add_binding(layout_set5, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    build_descriptor_set_layout(context.device, layout_set5);
    
    // create graphics pipeline
    if (0) {
        pipeline_description_t pipeline_description;
        init_graphics_pipeline_description(context, pipeline_description);
        add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT,   "main", "shaders/pbr.vert.spv");
        add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/pbr.frag.spv");
        graphics_pipeline.descriptor_set_layouts.push_back(layout_set0.handle);
        build_graphics_pipeline(context, pipeline_description, graphics_pipeline);
    }

    // create rt pipeline
    {
        rt_pipeline_description_t rt_pipeline_description;
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", "shaders/raytracing.rgen.spv");
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", "shaders/raytracing.rchit.spv");
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_MISS_BIT_KHR, "main", "shaders/raytracing.rmiss.spv");
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_MISS_BIT_KHR, "main", "shaders/shadow.rmiss.spv");

        shader_group_t group;
        // raygen
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.general = 0;
        group.closest_hit = VK_SHADER_UNUSED_KHR;
        group.any_hit = VK_SHADER_UNUSED_KHR;
        group.intersection = VK_SHADER_UNUSED_KHR;
        rt_pipeline_description.groups.push_back(group);
        // shadow miss and general miss
        group.general = 2;
        rt_pipeline_description.groups.push_back(group);
        group.general = 3;
        rt_pipeline_description.groups.push_back(group);
        // hit
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.general = 1;
        rt_pipeline_description.groups.push_back(group);
        rt_pipeline_description.max_recursion_depth = 3; // todo: check mas recursion depth
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set0.handle);
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set1.handle);
        build_raytracing_pipeline(context, rt_pipeline_description, &rtx_pipeline);
        // set region from group indices (todo: payloads)
        rt_pipeline_description.sbt_regions[RGEN_REGION] = {0};
        rt_pipeline_description.sbt_regions[CHIT_REGION] = {3};
        rt_pipeline_description.sbt_regions[MISS_REGION] = {1,2};
        build_shader_binding_table(context, rt_pipeline_description, rtx_pipeline, sbt);
    }

    // create morton encoder pipeline
    {
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/morton_encode.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set3.handle);
        build_compute_pipeline(context, compute_description, &morton_compute_pipeline);
    }

    //create bitonic sort step pipeline
    {
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/bitonic_sort.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set4.handle);
        build_compute_pipeline(context, compute_description, &sort_compute_pipeline);
    }

    // create tree builder pipeline
    {
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/light_tree.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set5.handle);
        build_compute_pipeline(context, compute_description, &tree_compute_pipeline);
    }
    
    // create post-process pipeline(s)
    {
        pipeline_description_t post_pipeline_description;
        init_graphics_pipeline_description(context, post_pipeline_description);
        add_shader(post_pipeline_description, VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/post.vert.spv");
        add_shader(post_pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/post.frag.spv");
        post_pipeline_description.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        post_pipeline.descriptor_set_layouts.push_back(layout_set2.handle);
        // todo: create sepearate renderpass (no depth buffer needed for this pipeline);
        build_graphics_pipeline(context, post_pipeline_description, post_pipeline);
    }
}

void renderer_t::update_descriptors(scene_t& scene)
{
    VkCommandBuffer cmd = begin_one_time_command_buffer(context, context.frames[0].command_pool);
    begin_upload(staging);
    frame_resources.resize(context.frames.size());
    for (size_t i = 0; i < context.frames.size(); ++i)
    {
        // set 0
        create_buffer(context, sizeof(camera_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_camera);
        create_buffer(context, MAX_LIGHTS * sizeof(light_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].ubo_light);
        create_buffer(context, MAX_ENTITIES * sizeof(model_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].ubo_model); 
        create_buffer(context, MAX_ENTITIES * sizeof(material_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].sbo_material);
        create_buffer(context, MAX_ENTITIES * sizeof(mesh_metadata_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].sbo_meshes);

        VkDescriptorBufferInfo ubo_camera_info = { frame_resources[i].ubo_camera.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_light_info  = { frame_resources[i].ubo_light.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_model_info  = { frame_resources[i].ubo_model.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo sbo_material_info = { frame_resources[i].sbo_material.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo sbo_meshes_info = { frame_resources[i].sbo_meshes.handle, 0, VK_WHOLE_SIZE };

        descriptor_set_t set0(set_layouts[0]);
        bind_buffer(set0, 0, &ubo_camera_info);
        bind_buffer(set0, 1, &ubo_light_info);
        bind_buffer(set0, 2, &ubo_model_info);
        bind_buffer(set0, 3, &sbo_material_info);
        bind_buffer(set0, 4, &sbo_meshes_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set0));

        // set 1
        create_buffer(context, sizeof(scene_info_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_scene);
        create_image(context, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, context.swapchain.extent.width, context.swapchain.extent.height, 
                VkImageUsageFlagBits(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),  VK_IMAGE_ASPECT_COLOR_BIT, 
                &frame_resources[i].storage_image);
        create_sampler(context, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &frame_resources[i].storage_image_sampler);
        VkDescriptorImageInfo  storage_image_info = { frame_resources[i].storage_image_sampler, frame_resources[i].storage_image.view, VK_IMAGE_LAYOUT_GENERAL };
        VkDescriptorBufferInfo ubo_scene_info = { frame_resources[i].ubo_scene.handle, 0, VK_WHOLE_SIZE };

        descriptor_set_t set1(set_layouts[1]);
        bind_acceleration_structure(set1, 0, &scene.tlas.handle);
        bind_buffer(set1, 1, &ubo_scene_info);
        bind_image(set1, 2, &storage_image_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set1));

        descriptor_set_t set2(set_layouts[2]);
        bind_image(set2, 0, &storage_image_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set2));

        // (morton encode)
        create_buffer(context, MAX_LIGHTS * sizeof(encoded_light_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_encoded_lights);
        VkDescriptorBufferInfo sbo_encoded_lights_info = { frame_resources[i].sbo_encoded_lights.handle, 0, VK_WHOLE_SIZE };
        descriptor_set_t set3(set_layouts[3]);
        bind_buffer(set3, 0, &ubo_light_info);
        bind_buffer(set3, 1, &sbo_encoded_lights_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set3));

        descriptor_set_t set4(set_layouts[4]);
        bind_buffer(set4, 0, &sbo_encoded_lights_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set4));

        create_buffer(context, MAX_LIGHT_TREE_SIZE * sizeof(node_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_light_tree);
        VkDescriptorBufferInfo sbo_light_tree_info = { frame_resources[i].sbo_light_tree.handle, 0, VK_WHOLE_SIZE };
        descriptor_set_t set5(set_layouts[5]);
        bind_buffer(set5, 0, &ubo_light_info);
        bind_buffer(set5, 1, &sbo_encoded_lights_info);
        bind_buffer(set5, 2, &sbo_light_tree_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set5));

        // create sync objects (todo: move this)
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (size_t i = 0; i < frame_resources.size(); i++)
        {
            VK_CHECK( vkCreateSemaphore(context.device, &semaphore_info, nullptr, &frame_resources[i].rt_semaphore) );
            VK_CHECK( vkCreateFence(context.device, &fence_info, nullptr, &frame_resources[i].rt_fence) );
        }

        // todo: use transfer queue for this part
        scene_info_t scene_info;
        scene_info.vertex_address = get_buffer_device_address(context, scene.vbo.handle);
        scene_info.index_address = get_buffer_device_address(context, scene.ibo.handle);
        copy_to_buffer(staging, frame_resources[i].ubo_scene, sizeof(scene_info_t), (void*)&scene_info, 0);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = frame_resources[i].storage_image.handle;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, 
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0, 
                0, nullptr,
                0, nullptr,
                1, &barrier);

        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.pNext = nullptr;
        alloc.commandPool = context.frames[i].command_pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        VK_CHECK( vkAllocateCommandBuffers(context.device, &alloc, &frame_resources[i].cmd) );
    }
    end_upload(staging); // uploaded to transfer
    end_and_submit_command_buffer(cmd, context.q_compute);
    VK_CHECK(wait_for_queue(context.q_compute));
    VK_CHECK(wait_for_queue(context.q_transfer));
    vkFreeCommandBuffers(context.device, context.frames[0].command_pool, 1, &cmd); 
    context.allocator.print_memory_usage();
}

renderer_t::~renderer_t()
{
    wait_idle(context);
    LOG_INFO("Descructor renderer");
    for (auto& f : frame_resources)
    {
        destroy_buffer(context, f.ubo_light);
        destroy_buffer(context, f.ubo_camera);
        destroy_buffer(context, f.ubo_model);
        destroy_buffer(context, f.sbo_material);
        destroy_buffer(context, f.sbo_meshes);
        destroy_buffer(context, f.ubo_scene);
        destroy_buffer(context, f.sbo_encoded_lights);
        destroy_buffer(context, f.sbo_light_tree);

        destroy_image(context, f.storage_image);
        vkDestroySampler(context.device, f.storage_image_sampler, nullptr);
    }

    LOG_INFO("Destroy set layouts");
    for (auto& l : set_layouts)
    {
        vkDestroyDescriptorSetLayout(context.device, l.handle, nullptr);
    }

    vkDestroyDescriptorPool(context.device, descriptor_allocator.descriptor_pool, nullptr);
    LOG_INFO("Destroy pipelines");
    //destroy_pipeline(context, &graphics_pipeline);
    destroy_pipeline(context, &rtx_pipeline);
    //destroy_staging_buffer(staging);
    destroy_context(context);
}

void renderer_t::draw_scene(scene_t& scene, camera_t& camera)
{
    static bool mat_uploaded[2] = {false, false};
    VkResult res;

    i32 frame_index;
    frame_t *frame;
    prepare_frame(context, &frame_index, &frame);
    VK_CHECK( vkWaitForFences(context.device, 1, &frame_resources[frame_index].rt_fence, VK_TRUE, ONE_SECOND_IN_NANOSECONDS) );
    vkResetFences(context.device, 1, &frame_resources[frame_index].rt_fence);
    get_next_swapchain_image(context, frame);
    
    {
        auto pool = frame->command_pool;
        VK_CHECK( vkResetCommandPool(context.device, pool, 0) );

        begin_upload(staging);
        if (!mat_uploaded[frame_index])
        {
            // copy materials
            copy_to_buffer(staging, frame_resources[frame_index].sbo_material, scene.materials.size() * sizeof(material_t), (void*)scene.materials.data(), 0);
            // copy mesh_data
            u64 offset = 0;
            for (const auto& mesh : scene.meshes)
            {
                mesh_metadata_t md;
                md.material_index = -1; // todo
                md.vertex_offset = mesh.vertex_offset;
                md.index_offset = mesh.index_offset;
                copy_to_buffer(staging, frame_resources[frame_index].sbo_meshes, sizeof(mesh_metadata_t), (void*)&md, offset);
                offset += sizeof(mesh_metadata_t);
            }
            
            mat_uploaded[frame_index] = true;
        }

        // lights
        copy_to_buffer(staging, frame_resources[frame_index].ubo_light, sizeof(light_t) * scene.lights.size(), (void*)scene.lights.data(), 0);
       
        // upload camera data
        camera_ubo_t camera_data;
        camera_data.position.xyz = camera.position; 
        camera_data.view = camera.m_view;
        camera_data.proj = camera.m_proj;
        camera_data.inv_view = inverse(camera.m_view);
        camera_data.inv_proj = inverse(camera.m_proj);
        copy_to_buffer(staging, frame_resources[frame_index].ubo_camera, sizeof(camera_ubo_t), (void*)&camera_data, 0);

        // upload model data
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
            data.material_index = entity.mesh_id; // todo: change
        }
        copy_to_buffer(staging, frame_resources[frame_index].ubo_model, model_data.size() * sizeof(model_t), (void*)model_data.data());
        
        VkSemaphore upload_complete;
        end_upload(staging, &upload_complete);
    
        // begin recording commands
        //VkCommandBuffer cmd = frame_resources[frame_index].comp_cmd;
        VkCommandBuffer cmd = frame->command_buffer;
        VK_CHECK( begin_command_buffer(cmd) );
        i32 num_lights = static_cast<i32>(scene.lights.size());

        // morton encoding
        if (ENABLE_MORTON_ENCODE) 
        {
            CHECKPOINT(cmd, "[PRE] MORTON ENCODING");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, morton_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, morton_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[3], 0, nullptr);
            vkCmdPushConstants(cmd, morton_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(i32), &num_lights);
            u32 threads = u32(num_lights)/512;
            vkCmdDispatch(cmd, threads, 1, 1);

            // memory barrier to wait for finish morton encoding
            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            CHECKPOINT(cmd, "[POST] MORTON ENCODING");
        }

        // bitonic sort lights
        if (ENABLE_SORT_LIGHTS) 
        {
            CHECKPOINT(cmd, "[PRE] BITONIC SORT");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sort_compute_pipeline.handle);
            // todo: create descriptor set
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sort_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[4], 0, nullptr);
            int n = num_lights;
            struct constants_t
            {
                int j;
                int k;
            } constants;

            for (int k = 2; k <= n; k <<= 1) 
            {
                for (int j = k >> 1; j > 0; j >>= 1)
                {
                    constants.j = j;
                    constants.k = k;
                    vkCmdPushConstants(cmd, sort_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants_t), &constants);
                    vkCmdDispatch(cmd, n, 1, 1);

                    // wait for prev to finish
                    VkMemoryBarrier barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
                }
            }
            CHECKPOINT(cmd, "[POST] BITONIC SORT");
        }

        // build light tree
        if (ENABLE_LIGHT_TREE) 
        {
            CHECKPOINT(cmd, "[PRE] LIGHT TREE BUILD");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[5], 0, nullptr);
            u32 groups = static_cast<u32>(log2(num_lights)) + 1;
            vkCmdDispatch(cmd, groups, 1, 1);

            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
            CHECKPOINT(cmd, "[POST] LIGHT TREE BUILD");
        }

        // render using light tree
        if (ENABLE_RTX)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx_pipeline.layout, 0, 
                    2, &frame_resources[frame_index].descriptor_sets[0], 0, nullptr);
            vkCmdPushConstants(cmd, rtx_pipeline.layout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(i32), &num_lights);
            CHECKPOINT(cmd, "[PRE] RAY TRACE");
            vkCmdTraceRays(cmd, &sbt.rgen, &sbt.miss, &sbt.hit, &sbt.call, context.swapchain.extent.width, context.swapchain.extent.height, 1);
            CHECKPOINT(cmd, "[POST] RAY TRACE");
        }

        VK_CHECK( vkEndCommandBuffer(cmd) );
        VkPipelineStageFlags dst_wait_mask[2] = { VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR };
        VkSemaphore wait_sempahores[2] = { frame->present_semaphore, upload_complete };
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.waitSemaphoreCount = 2;
        submit_info.pWaitSemaphores = wait_sempahores;
        submit_info.pWaitDstStageMask = dst_wait_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame_resources[frame_index].rt_semaphore;

        VK_CHECK( vkQueueSubmit(context.q_compute, 1, &submit_info, frame_resources[frame_index].rt_fence) );

        if (0) 
        {
            // compute to local
            cmd = frame_resources[frame_index].cmd;
            VK_CHECK( begin_command_buffer(cmd) ); 
            buffer_t local;
            u32 h = static_cast<u32>(log2(scene.lights.size()));
            u32 node_count = ((1 << (h + 1)) - 1);
            u32 size = sizeof(node_t) *node_count;
            create_buffer(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &local,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            VkBufferCopy copy{};
            copy.srcOffset = 0;
            copy.dstOffset = 0;
            copy.size = size;
           
            vkCmdCopyBuffer(cmd, frame_resources[frame_index].sbo_light_tree.handle, local.handle, 1, &copy);
            VK_CHECK( vkEndCommandBuffer(cmd) );

            VkPipelineStageFlags _dst_wait_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &frame_resources[frame_index].rt_semaphore;
            submit_info.pWaitDstStageMask = &_dst_wait_mask;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &frame->render_semaphore;
            submit_info.pCommandBuffers = &cmd;

            VK_CHECK( vkQueueSubmit(context.q_transfer, 1, &submit_info, frame->fence) ); 
            VK_CHECK( wait_for_queue(context.q_transfer) );

            u8* p_data;
            context.allocator.map_memory(local.allocation, (void**)&p_data);
            auto* ptr = reinterpret_cast<node_t*>(p_data);
            for (size_t i = 0; i < node_count; i++)
            {
                LOG_INFO("node: id %d, I %f", ptr[i].id, ptr[i].intensity);
            }
            context.allocator.unmap_memory(local.allocation);
            abort();
        }
       
        cmd = frame_resources[frame_index].cmd;
        VK_CHECK( begin_command_buffer(cmd) ); 
        VkClearValue clear[2];
        clear[0].color = {0, 0, 0, 0};
        clear[1].depthStencil = {1, 0};
        begin_render_pass(context, cmd, clear, 2);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline.layout, 0,
                1, &frame_resources[frame_index].descriptor_sets[2], 0, nullptr);
        vkCmdDraw(cmd, 4, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
        VK_CHECK( vkEndCommandBuffer(cmd) );

        VkPipelineStageFlags _dst_wait_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &frame_resources[frame_index].rt_semaphore;
        submit_info.pWaitDstStageMask = &_dst_wait_mask;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame->render_semaphore;
        submit_info.pCommandBuffers = &cmd;

        VK_CHECK( vkQueueSubmit(context.q_graphics, 1, &submit_info, frame->fence) ); 
#if 0
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
#endif
    }

    //graphics_submit_frame(context, frame);
    present_frame(context, frame);
}


