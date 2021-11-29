#include "ui.h"
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include "renderer.h"
#include "scene.h"

#include <cstdio>

void init_imgui(imgui_t& imgui, gpu_context_t& ctx, VkRenderPass render_pass, window_t*  window)
{
    init_descriptor_allocator(ctx.device, 2, &imgui.descriptor_allocator);
	ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window->window, true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = ctx.instance;
	init_info.PhysicalDevice = ctx.physical_device;
	init_info.Device = ctx.device;
	init_info.Queue = ctx.q_graphics;
	init_info.DescriptorPool = imgui.descriptor_allocator.descriptor_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_Init(&init_info, render_pass);

    VkCommandBuffer cmd = ctx.frames[0].command_buffer;
    begin_command_buffer(cmd);
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    end_and_submit_command_buffer(cmd, ctx.q_graphics);
    VK_CHECK( wait_for_queue(ctx.q_graphics) );
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void destroy_imgui(imgui_t& imgui)
{
    destroy_descriptor_allocator(imgui.descriptor_allocator);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// inside main loop
bool new_frame(imgui_t& imgui, render_state_t* state, scene_t* scene, frame_time_t& time)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Render settings");
    ImGui::Text("FPS: %d", get_average_fps(time));
    ImGui::Checkbox("Render lights",  &state->render_lights);
    ImGui::Checkbox("Render bounding boxes",  &state->render_bboxes);
    if (!state->render_bboxes) 
        ImGui::BeginDisabled();
    ImGui::Checkbox("Only render selected bbox nodes",  &state->render_only_selected_nodes);
    if (!state->render_bboxes) 
        ImGui::EndDisabled();
    ImGui::SliderInt("Cut size", &state->num_samples, 1, MIN(static_cast<i32>(scene->lights.size()), 32));
    ImGui::Checkbox("Random lights", &state->use_random_lights);
    if (state->use_random_lights)
    {
        ImGui::SliderInt("Num random lights", &state->num_random_lights, 1, MAX_LIGHTS);
        ImGui::SliderFloat("Distance from scene", &state->distance_from_origin, 1.0f, 100.0f);
    }
    ImVec2 region = ImGui::GetContentRegionAvail();
    region.y = MIN(region.y, 150);

    if (!state->use_random_lights && ImGui::BeginListBox("Lights", region))
    {
        ImGui::Text("Scene");
        if (ImGui::BeginTable("Lights Table", 2))
        {
            ImGuiColorEditFlags flags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel;
            int i = 0;
            for (auto& light : scene->lights)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char buf[50];
                sprintf(buf, "%d", i++); 
                ImGui::ColorEdit3(buf, light.color.data, flags);
                ImGui::TableNextColumn();
                const char* label = "light";
                ImGui::Text("%s", label);
            }
            ImGui::EndTable();
        }
        ImGui::EndListBox();
    }
    if (ImGui::Button("Add light"))
    {
        v3 dir = normalize(vec3(_randf2(), _randf(), _randf2()));
        v3 pos = dir * _randf() * 5.0f;
        add_light(*scene, pos, vec3(1));
    }
    ImGui::End();
    ImGui::Render();
    return ImGui::IsAnyMouseDown() && (ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow));
}

// called inside renderer at the end just before presenting (after post process)
void render_imgui(VkCommandBuffer cmd, imgui_t& imgui)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
