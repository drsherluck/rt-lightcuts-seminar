#ifndef RENDERER_H
#define RENDERER_H

#include "backend.h"
#include "staging.h"
#include "pipeline.h"
#include "descriptor.h"
#include "profiler.h"

struct scene_t;
struct camera_t;

struct frame_resource_t
{
    std::vector<VkDescriptorSet> descriptor_sets;
    buffer_t ubo_light;
    buffer_t ubo_camera;
    buffer_t ubo_model;
    buffer_t sbo_material;
    buffer_t sbo_meshes;

    buffer_t ubo_scene;
    image_t  storage_image;
    VkSampler storage_image_sampler;

    buffer_t sbo_encoded_lights;
    buffer_t sbo_light_tree;

    // syncs
    VkFence rt_fence;
    VkSemaphore rt_semaphore;

    VkCommandBuffer cmd;
};

struct renderer_t
{
    window_t*              window;
    gpu_context_t          context;
    profiler_t             profiler;
    pipeline_t             graphics_pipeline;
    pipeline_t             rtx_pipeline;
    pipeline_t             post_pipeline;
    
    // compute pipelines
    pipeline_t             morton_compute_pipeline;
    pipeline_t             sort_compute_pipeline;
    pipeline_t             tree_compute_pipeline;
    shader_binding_table_t sbt;
    staging_buffer_t       staging;

    std::vector<frame_resource_t> frame_resources;
    descriptor_allocator_t descriptor_allocator;
    std::vector<descriptor_set_layout_t> set_layouts;

    renderer_t(window_t* _window);
    ~renderer_t();

    void draw_scene(scene_t& scene, camera_t& camera);
    void update_descriptors(scene_t& scene);
};

#endif // RENDERER_H
