#ifndef RENDERER_H
#define RENDERER_H

#include "backend.h"
#include "staging.h"
#include "pipeline.h"

struct scene_t;
struct camera_t;

struct frame_resource_t
{
    std::vector<VkDescriptorSet> descriptor_sets;
    buffer_t ubo_light;
    buffer_t ubo_camera;
    buffer_t ubo_model;
    buffer_t sbo_material;
};

struct renderer_t
{
    window_t*     window;
    gpu_context_t context;
    pipeline_t    graphics_pipeline;
    staging_buffer_t staging;

    std::vector<frame_resource_t> frame_resources;

    renderer_t(window_t* _window);
    ~renderer_t();

    void draw_scene(scene_t& scene, camera_t& camera);
};

#endif // RENDERER_H
