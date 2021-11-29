#ifndef UI_H
#define UI_H

#include "backend.h"
#include "descriptor.h"
#include "staging.h"
#include "pipeline.h"
#include "window.h"

#include <imgui.h>

struct render_state_t;
struct scene_t;

struct imgui_t
{
    descriptor_allocator_t descriptor_allocator;
};

void init_imgui(imgui_t& imgui, gpu_context_t& context, VkRenderPass render_pass, window_t* window);
void render_imgui(VkCommandBuffer cmd, imgui_t& imgui);
void destroy_imgui(imgui_t& imgui);
bool new_frame(imgui_t& imgui, render_state_t* state, scene_t* scene);

#endif // UI_H
