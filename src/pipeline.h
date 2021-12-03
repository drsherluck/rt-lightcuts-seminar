#ifndef PIPELINE_H
#define PIPELINE_H

#include "common.h"
#include "backend.h"

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
    bool                  depth_test = true;
    bool                  depth_write = true;
    std::vector<VkDynamicState> dynamic_states;
    std::vector<descriptor_set_data_t> descriptor_sets;
	std::vector<VkPushConstantRange>   push_constants;
    std::vector<VkVertexInputBindingDescription>   binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
};

struct pipeline_t
{
    VkPipeline                         handle;
    VkPipelineLayout                   layout;
    //pipeline_description_t             description; // for swapchain recreation
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

enum sbt_region
{
    RGEN_REGION = 0,
    CHIT_REGION = 1,   
    MISS_REGION = 2,
    CALL_REGION = 3
};

typedef std::vector<u32> shader_binding_table_regions[4];

struct rt_pipeline_description_t
{
    u32                         max_recursion_depth;
    std::vector<shader_t>       shaders;
    std::vector<shader_group_t> groups;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange>   push_constants;
    shader_binding_table_regions sbt_regions;
};

struct shader_binding_table_t
{
    buffer_t buffer;
    VkStridedDeviceAddressRegionKHR rgen;
    VkStridedDeviceAddressRegionKHR hit;
    VkStridedDeviceAddressRegionKHR miss;
    VkStridedDeviceAddressRegionKHR call;
};

struct compute_pipeline_description_t 
{
    shader_t shader;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constants;
};

struct gpu_context_t;

void init_graphics_pipeline_description(gpu_context_t& ctx, pipeline_description_t& description);
void add_shader(pipeline_description_t& description, VkShaderStageFlagBits stage, std::string entry, const char* path);
bool build_graphics_pipeline(gpu_context_t& ctx, pipeline_description_t& description, pipeline_t& pipeline);

bool build_raytracing_pipeline(gpu_context_t& ctx, rt_pipeline_description_t& description, pipeline_t* pipeline);
bool build_shader_binding_table(gpu_context_t& ctx, rt_pipeline_description_t& description, pipeline_t& pipeline, shader_binding_table_t& sbt);
void destroy_shader_binding_table(gpu_context_t& context, shader_binding_table_t& sbt);
void add_shader(rt_pipeline_description_t& description, VkShaderStageFlagBits stage, std::string entry, const char* path);

bool build_compute_pipeline(gpu_context_t& ctx, compute_pipeline_description_t&, pipeline_t* pipeline);
void add_shader(compute_pipeline_description_t& description, std::string entry, const char* path);

void destroy_pipeline(gpu_context_t& ctx, pipeline_t* pipeline);
#endif
