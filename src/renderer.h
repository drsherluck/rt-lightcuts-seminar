#ifndef RENDERER_H
#define RENDERER_H

#include "backend.h"
#include "staging.h"
#include "pipeline.h"
#include "descriptor.h"
#include "profiler.h"
#include "ui.h"

#define MAX_LIGHTS_SAMPLED 32
#define MAX_ENTITIES 100
#define MAX_TREE_HEIGTH 17
#define MAX_LIGHTS (1 << MAX_TREE_HEIGTH)
#define MAX_LIGHT_TREE_SIZE (1 << (MAX_TREE_HEIGTH + 1))

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

    buffer_t ubo_bounds;
    buffer_t sbo_encoded_lights;
    buffer_t sbo_light_tree;

    // here are bbox lines writen
    buffer_t vbo_lines;
    buffer_t sbo_nodes_highlight;
    buffer_t sbo_leaf_select;

    // lines for visualizing sampling
    buffer_t vbo_ray_lines;

    // syncs
    VkFence rt_fence;
    VkSemaphore rt_semaphore;
    VkSemaphore prepass_semaphore;

    VkCommandBuffer cmd;
    VkCommandBuffer cmd_prepass;
    VkCommandBuffer cmd_dwn;
};

// config for what to render or to input into push_constants
// that can be changed at runtime
struct render_state_t
{
    i32  cut_size = 1;
    i32  num_samples = 1;
    bool render_depth_buffer = false;
    bool render_sample_lines = false; 
    v2   screen_uv; 
    
    bool render_lights = true;
    bool render_bboxes = false;
    bool render_only_selected_nodes = false;
    bool render_step_mode = false;
    i32  step = 0;

    bool use_random_lights = false;
    i32  num_random_lights = 1;
    f32  distance_from_origin = 1;

    // todo
    bool paused = false;
  
    // debugging info passed on for imgui to use
    struct {
        i32 id;
        i32 selected;
    } cut[MAX_LIGHT_TREE_SIZE] = {};
    i32 selected_leafs[MAX_LIGHTS] = {};
};

struct renderer_t
{
    window_t*              window;
    gpu_context_t          context;
    profiler_t             profiler;
    imgui_t                imgui;
  
    // todo convert to texture_t
    image_t                color_attachment[BUFFERED_FRAMES];
    VkSampler              color_sampler[BUFFERED_FRAMES];
    image_t                depth_attachment[BUFFERED_FRAMES];
    VkSampler              depth_sampler[BUFFERED_FRAMES];
    VkFramebuffer          prepass_framebuffer[BUFFERED_FRAMES];
    VkRenderPass           prepass_render_pass;
    
    VkFramebuffer          bbox_framebuffers[BUFFERED_FRAMES];
    VkRenderPass           bbox_render_pass;

    pipeline_t             prepass_pipeline;
    pipeline_t             debug_pipeline;
    pipeline_t             rtx_pipeline;
    pipeline_t             query_pipeline;
    pipeline_t             post_pipeline;
    pipeline_t             points_pipeline;
    pipeline_t             lines_pipeline;
    
    // compute pipelines
    pipeline_t             morton_compute_pipeline;
    pipeline_t             sort_compute_pipeline;
    pipeline_t             tree_leafs_compute_pipeline;
    pipeline_t             tree_compute_pipeline;
    pipeline_t             bbox_lines_pso;
    shader_binding_table_t sbt;
    shader_binding_table_t query_sbt;
    staging_buffer_t       staging;

    // used to clear some gpu buffers
    void* empty_buffer = nullptr;

    // shared by all frames 
    buffer_t ray_lines_info;

    std::vector<frame_resource_t> frame_resources;
    descriptor_allocator_t descriptor_allocator;
    std::vector<descriptor_set_layout_t> set_layouts;

    renderer_t(window_t* _window);
    ~renderer_t();

    void draw_scene(scene_t& scene, camera_t& camera, render_state_t& state);
    void update_descriptors(scene_t& scene);
    void create_prepass_render_pass();
    void create_bbox_render_pass();
};

#endif // RENDERER_H
