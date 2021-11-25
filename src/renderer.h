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

    // here are bbox lines writen
    buffer_t vbo_lines;

    // syncs
    VkFence rt_fence;
    VkSemaphore rt_semaphore;
    VkSemaphore prepass_semaphore;

    VkCommandBuffer cmd;
    VkCommandBuffer cmd_prepass;
};

// config for what to render or to input into push_constants
// that can be changed at runtime
struct render_state_t
{
    bool render_depth_buffer;
};

struct renderer_t
{
    window_t*              window;
    gpu_context_t          context;
    profiler_t             profiler;
   
    image_t                depth_attachment[BUFFERED_FRAMES];
    VkSampler              depth_sampler[BUFFERED_FRAMES];
    VkFramebuffer          prepass_framebuffer[BUFFERED_FRAMES];
    VkRenderPass           prepass_render_pass;
    
    VkFramebuffer          bbox_framebuffers[BUFFERED_FRAMES];
    VkRenderPass           bbox_render_pass;

    pipeline_t             prepass_pipeline;
    pipeline_t             debug_pipeline;
    pipeline_t             rtx_pipeline;
    pipeline_t             post_pipeline;
    pipeline_t             bbox_pipeline;
    pipeline_t             points_pipeline;
    
    // compute pipelines
    pipeline_t             morton_compute_pipeline;
    pipeline_t             sort_compute_pipeline;
    pipeline_t             tree_leafs_compute_pipeline;
    pipeline_t             tree_compute_pipeline;
    pipeline_t             bbox_lines_pso;
    shader_binding_table_t sbt;
    staging_buffer_t       staging;

    std::vector<frame_resource_t> frame_resources;
    descriptor_allocator_t descriptor_allocator;
    std::vector<descriptor_set_layout_t> set_layouts;

    renderer_t(window_t* _window);
    ~renderer_t();

    void draw_scene(scene_t& scene, camera_t& camera, render_state_t state);
    void update_descriptors(scene_t& scene);
    void create_prepass_render_pass();
    void create_bbox_render_pass();
};

#endif // RENDERER_H
