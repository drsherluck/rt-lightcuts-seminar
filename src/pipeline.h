#ifndef PIPELINE_H
#define PIPELINE_H

#include "common.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct shader_t
{
    std::string           entry;
    std::vector<u8>       code;
    VkShaderStageFlagBits stage;
};

struct descriptor_set_data_t
{
    u32                                       set_id;
    VkDescriptorSetLayoutCreateInfo           create_info;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct pipeline_description_t
{
	std::vector<shader_t> shaders;
	VkPrimitiveTopology   topology;
	VkPolygonMode         polygon_mode;
	VkCullModeFlags       cull_mode;
	VkViewport            viewport;
	VkRect2D              sciccor;
    VkRenderPass          render_pass;
    bool                  color_blending = false;
    std::vector<descriptor_set_data_t> descriptor_sets;
	std::vector<VkPushConstantRange>   push_constants;
    std::vector<VkVertexInputBindingDescription>   binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
};

struct pipeline_t
{
    VkPipeline                         handle;
    VkPipelineLayout                   layout;
    pipeline_description_t             description;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
};

struct shader_group_t
{
    VkRayTracingShaderGroupTypeKHR type;
    u32 general;
    u32 closest_hit;
    u32 any_hit;
    u32 intersection;
};

struct rt_pipeline_description_t
{
    u32                         max_recursion_depth;
    std::vector<shader_t>       shaders;
    std::vector<shader_group_t> groups;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange>   push_constants;
};

struct gpu_context_t;

inline bool get_binding_layout(pipeline_t& pipeline, u32 set, u32 binding_id, VkDescriptorSetLayoutBinding& out)
{

    for (const auto& layout : pipeline.description.descriptor_sets[set].bindings)
    {
        if (layout.binding == binding_id)
        {
            out = layout;
            return true;
        }
    }
    return false;
}


void init_graphics_pipeline_description(gpu_context_t& ctx, pipeline_description_t& description);
void add_shader(pipeline_description_t& description, VkShaderStageFlagBits stage, std::string entry, const char* path);
bool build_graphics_pipeline(gpu_context_t& ctx, pipeline_description_t& description, pipeline_t& pipeline);

#endif
