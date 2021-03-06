#include "renderer.h"
#include "scene.h"
#include "camera.h"
#include "debug_util.h"
#include "shader_data.h"
#include "time.h"
#include "window.h"
#include "ui.h"

#include <cassert>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define CLOCK_REALTIME 1

// port clock_gettime on windows (quick fix)
// https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
int clock_gettime(int, struct timespec* tv)
{
    static int initialized = 0;
    static LARGE_INTEGER freq, start_count;
    static struct timespec tv_start;
    LARGE_INTEGER cur_count;
    time_t sec_part;
    time_t nsec_part;

    if (!initialized)
    {
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&start_count);
        timespec_get(&tv_start, TIME_UTC);
        initialized = 1;
    }
	QueryPerformanceCounter(&cur_count);
    cur_count.QuadPart -= start_count.QuadPart;
    sec_part = cur_count.QuadPart / freq.QuadPart;
    nsec_part = (long)((cur_count.QuadPart - (sec_part * freq.QuadPart))
        * 1000000000UL / freq.QuadPart);
    tv->tv_sec = tv_start.tv_sec + sec_part;
    tv->tv_nsec = tv_start.tv_nsec + nsec_part;
    if (tv->tv_nsec >= 1000000000UL) 
    {
        tv->tv_sec += 1;
        tv->tv_nsec -= 1000000000UL;
    }
    return 0;
}
#endif

#define ENABLE_MORTON_ENCODE 1
#define ENABLE_SORT_LIGHTS 1
#define ENABLE_LIGHT_TREE 1
#define ENABLE_BBOX_DEBUG 1
#define ENABLE_RTX 1
#define ENABLE_VERIFY 1

#define MAX_DESCRIPTOR_SETS 50

struct batch_t 
{
    u32 instance_count;
    u32 mesh_id;
};

void renderer_t::create_prepass_render_pass()
{
    // create render pass;
	VkAttachmentDescription attachment[2] = {};
    // depth
	attachment[0].format         = VK_FORMAT_D32_SFLOAT; 
	attachment[0].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachment[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachment[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    // albedo
	attachment[1].format         = VK_FORMAT_R32G32B32A32_SFLOAT; 
	attachment[1].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachment[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachment[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment[1].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref =  {};
    depth_ref.attachment = 0;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
	VkAttachmentReference color_ref =  {};
    color_ref.attachment = 1;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount    = 0;
	subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependencies[3] = {};
	dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass      = 0;
	dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[2].dstSubpass      = 0;
	dependencies[2].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].srcAccessMask   = 0;
	dependencies[2].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dependencyFlags = 0;


	VkRenderPassCreateInfo create_info = {};
	create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.attachmentCount = 2;
	create_info.pAttachments    = attachment;
	create_info.subpassCount    = 1;
	create_info.pSubpasses      = &subpass;
	create_info.dependencyCount = 3;
	create_info.pDependencies   = dependencies;

	VK_CHECK( vkCreateRenderPass(context.device, &create_info, nullptr, &prepass_render_pass) );
	LOG_INFO("Prepass renderpass created");

    // create framebuffer
    for (i32 i = 0; i < BUFFERED_FRAMES; ++i)
    {
        // depth
        create_image(context, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
                context.swapchain.extent.width, context.swapchain.extent.height,
                VkImageUsageFlagBits(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                VK_IMAGE_ASPECT_DEPTH_BIT,
                &depth_attachment[i]);
        create_sampler(context, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &depth_sampler[i]);
        
        // color 
        create_image(context, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT,
                context.swapchain.extent.width, context.swapchain.extent.height,
                VkImageUsageFlagBits(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                VK_IMAGE_ASPECT_COLOR_BIT,
                &color_attachment[i]);
        create_sampler(context, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &color_sampler[i]);

        VkImageView views[2] = { depth_attachment[i].view, color_attachment[i].view };
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = prepass_render_pass;
		info.attachmentCount = 2;
		info.pAttachments = views;
		info.width  = context.swapchain.extent.width;
		info.height = context.swapchain.extent.height;
		info.layers = 1;

		VK_CHECK( vkCreateFramebuffer(context.device, &info, nullptr, &prepass_framebuffer[i]) );
    }
    LOG_INFO("Prepass framebuffers created");
}

void renderer_t::create_bbox_render_pass()
{
    // create render pass;
    VkAttachmentDescription attachment[2] = {};
	attachment[0].format         = VK_FORMAT_D32_SFLOAT; 
	attachment[0].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachment[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD; // use prepass depth
	attachment[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachment[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment[0].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	attachment[0].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	attachment[1].format         = VK_FORMAT_R32G32B32A32_SFLOAT; 
	attachment[1].samples        = VK_SAMPLE_COUNT_1_BIT;
	attachment[1].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD; // use rendered image from rtx pipeline
	attachment[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	attachment[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment[1].initialLayout  = VK_IMAGE_LAYOUT_GENERAL;
	attachment[1].finalLayout    = VK_IMAGE_LAYOUT_GENERAL;

	VkAttachmentReference depth_ref =  {};
    depth_ref.attachment = 0;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
	VkAttachmentReference color_ref =  {};
    color_ref.attachment = 1;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount    = 0;
	subpass.colorAttachmentCount    = 1;
	subpass.pColorAttachments       = &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependencies[3] = {};
	dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass      = 0;
	dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
	dependencies[2].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[2].dstSubpass      = 0;
	dependencies[2].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].srcAccessMask   = 0;
	dependencies[2].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dependencyFlags = 0;


	VkRenderPassCreateInfo create_info = {};
	create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.attachmentCount = 2;
	create_info.pAttachments    = attachment;
	create_info.subpassCount    = 1;
	create_info.pSubpasses      = &subpass;
	create_info.dependencyCount = 3;
	create_info.pDependencies   = dependencies;

	VK_CHECK( vkCreateRenderPass(context.device, &create_info, nullptr, &bbox_render_pass) );
	LOG_INFO("Prepass renderpass created");

    // create framebuffer
    for (i32 i = 0; i < BUFFERED_FRAMES; ++i)
    {
        create_image(context, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, context.swapchain.extent.width, context.swapchain.extent.height, 
                VkImageUsageFlagBits(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),  VK_IMAGE_ASPECT_COLOR_BIT, 
                &frame_resources[i].storage_image);
        create_sampler(context, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &frame_resources[i].storage_image_sampler);

        VkImageView attachments[2] = { depth_attachment[i].view, frame_resources[i].storage_image.view };
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = bbox_render_pass;
		info.attachmentCount = 2;
		info.pAttachments = attachments;
		info.width  = context.swapchain.extent.width;
		info.height = context.swapchain.extent.height;
		info.layers = 1;

		VK_CHECK( vkCreateFramebuffer(context.device, &info, nullptr, &bbox_framebuffers[i]) );
    }
    LOG_INFO("Prepass framebuffers created");
}

renderer_t::renderer_t(window_t* _window) : window(_window)
{
    init_context(context, window);
    init_profiler(context.device, profiler);
    init_staging_buffer(staging, &context);
    init_descriptor_allocator(context.device, MAX_DESCRIPTOR_SETS,  &descriptor_allocator);
    frame_resources.resize(context.frames.size());
    create_prepass_render_pass();
    create_bbox_render_pass();
    
    // create descriptor layouts for pipelines
    // set 0 
    set_layouts.resize(9);
    auto& layout_set0 = set_layouts[0];
    add_binding(layout_set0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    add_binding(layout_set0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT   | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set0, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
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
    add_binding(layout_set3, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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
    // set 6 (write vbo lines compute shader)
    auto& layout_set6 = set_layouts[6];
    add_binding(layout_set6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    add_binding(layout_set6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    build_descriptor_set_layout(context.device, layout_set6);
    // set 7 (debugging info)
    auto& layout_set7 = set_layouts[7];
    add_binding(layout_set7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set7, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    add_binding(layout_set7, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // indices of nodes in tree that highlight
    add_binding(layout_set7, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); // selected leaf nodes
    build_descriptor_set_layout(context.device, layout_set7);
    // set 8 flags for highlight bbox nodes
    auto& layout_set8 = set_layouts[8];
    add_binding(layout_set8, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    build_descriptor_set_layout(context.device, layout_set8);
    
    // create prepass pipeline
    {
        pipeline_description_t pipeline_description;
        init_graphics_pipeline_description(context, pipeline_description);
        add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT,   "main", "shaders/prepass.vert.spv");
        add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT,   "main", "shaders/prepass.frag.spv");
        pipeline_description.render_pass = prepass_render_pass;
        prepass_pipeline.descriptor_set_layouts.push_back(layout_set0.handle); // dont care
        build_graphics_pipeline(context, pipeline_description, prepass_pipeline);
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
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set7.handle); // debuging lines
        build_raytracing_pipeline(context, rt_pipeline_description, &rtx_pipeline);
        // set region from group indices (todo: payloads)
        rt_pipeline_description.sbt_regions[RGEN_REGION] = {0};
        rt_pipeline_description.sbt_regions[CHIT_REGION] = {3};
        rt_pipeline_description.sbt_regions[MISS_REGION] = {1,2};
        build_shader_binding_table(context, rt_pipeline_description, rtx_pipeline, sbt);
    }

    // create hit query pipeline (there is no query support for 1060 6gb)
    {
        rt_pipeline_description_t rt_pipeline_description;
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", "shaders/hit_query.rgen.spv");
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", "shaders/hit_query.rchit.spv");
        add_shader(rt_pipeline_description, VK_SHADER_STAGE_MISS_BIT_KHR, "main", "shaders/hit_query.rmiss.spv");

        shader_group_t group;
        // raygen
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.general = 0;
        group.closest_hit = VK_SHADER_UNUSED_KHR;
        group.any_hit = VK_SHADER_UNUSED_KHR;
        group.intersection = VK_SHADER_UNUSED_KHR;
        rt_pipeline_description.groups.push_back(group);
        // general miss
        group.general = 2;
        rt_pipeline_description.groups.push_back(group);
        // hit
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.general = 1;
        rt_pipeline_description.groups.push_back(group);
        rt_pipeline_description.max_recursion_depth = 3; // todo: check mas recursion depth
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set0.handle);
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set1.handle);
        rt_pipeline_description.descriptor_set_layouts.push_back(layout_set7.handle); // debuging lines
        build_raytracing_pipeline(context, rt_pipeline_description, &query_pipeline);
        // set region from group indices (todo: payloads)
        rt_pipeline_description.sbt_regions[RGEN_REGION] = {0};
        rt_pipeline_description.sbt_regions[CHIT_REGION] = {2};
        rt_pipeline_description.sbt_regions[MISS_REGION] = {1};
        build_shader_binding_table(context, rt_pipeline_description, query_pipeline, query_sbt);
    }

    // create morton encoder pipeline
    {
        LOG_INFO("Create morton encoder pipeline");
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/morton_encode.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set3.handle);
        build_compute_pipeline(context, compute_description, &morton_compute_pipeline);
    }

    //create bitonic sort step pipeline
    {
        LOG_INFO("Create bitonic sort pipeline");
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/bitonic_sort.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set4.handle);
        build_compute_pipeline(context, compute_description, &sort_compute_pipeline);
    }

    // create tree leaves builder pipeline
    {
        LOG_INFO("Create leaf nodes pipeline");
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/light_tree_leaf_nodes.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set5.handle);
        build_compute_pipeline(context, compute_description, &tree_leafs_compute_pipeline);
    }

    // create tree builder pipeline
    {
        LOG_INFO("Create inner nodes pipeline");
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/light_tree.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set5.handle);
        build_compute_pipeline(context, compute_description, &tree_compute_pipeline);
    }
    
    // create post-process pipeline(s)
    {
        LOG_INFO("Create post-process pipeline");
        pipeline_description_t post_pipeline_description;
        init_graphics_pipeline_description(context, post_pipeline_description);
        add_shader(post_pipeline_description, VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/post.vert.spv");
        add_shader(post_pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/post.frag.spv");
        post_pipeline_description.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        post_pipeline.descriptor_set_layouts.push_back(layout_set2.handle);
        // todo: create sepearate renderpass (no depth buffer needed for this pipeline);
        build_graphics_pipeline(context, post_pipeline_description, post_pipeline);
    }

    // debug
    {
        LOG_INFO("Create debug prepass viewer pipeline");
        pipeline_description_t pipeline_description;
        init_graphics_pipeline_description(context, pipeline_description);
        add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/post.vert.spv");
        add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/debug.frag.spv");
        pipeline_description.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        debug_pipeline.descriptor_set_layouts.push_back(layout_set2.handle);
        build_graphics_pipeline(context, pipeline_description, debug_pipeline);
    }

    // compute shader to write bbox lines to a buffer
    {
        LOG_INFO("Create aabb lines generator pipeline");
        compute_pipeline_description_t compute_description;
        add_shader(compute_description, "main", "shaders/bbox_lines.comp.spv");
        compute_description.descriptor_set_layouts.push_back(layout_set6.handle);
        build_compute_pipeline(context, compute_description, &bbox_lines_pso);
    }

    // bbox visualizer
    {
        LOG_INFO("Create lines pipeline");
        pipeline_description_t pipeline_description;
        init_graphics_pipeline_description(context, pipeline_description);
        add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/bbox.vert.spv");
        add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/bbox.frag.spv");
        pipeline_description.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        pipeline_description.polygon_mode = VK_POLYGON_MODE_LINE;
        pipeline_description.cull_mode = VK_CULL_MODE_NONE;
        pipeline_description.render_pass = bbox_render_pass;
        lines_pipeline.descriptor_set_layouts.push_back(layout_set0.handle); // need cameraubo
        lines_pipeline.descriptor_set_layouts.push_back(layout_set8.handle); // nodes to highlight
        build_graphics_pipeline(context, pipeline_description, lines_pipeline);
    }

    // point light visualizer
    {
        LOG_INFO("Create points pipeline");
        pipeline_description_t pipeline_description;
        init_graphics_pipeline_description(context, pipeline_description);
        add_shader(pipeline_description, VK_SHADER_STAGE_VERTEX_BIT, "main", "shaders/points.vert.spv");
        add_shader(pipeline_description, VK_SHADER_STAGE_FRAGMENT_BIT, "main", "shaders/points.frag.spv");
        pipeline_description.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        pipeline_description.polygon_mode = VK_POLYGON_MODE_POINT;
        pipeline_description.render_pass = bbox_render_pass;
        points_pipeline.descriptor_set_layouts.push_back(layout_set0.handle); // need cameraubo
        build_graphics_pipeline(context, pipeline_description, points_pipeline);
    }

    init_imgui(imgui, context, context.render_pass, window);
}


// Creates the descriptor sets and their respective buffers and images
void renderer_t::update_descriptors(scene_t& scene)
{
    VkCommandBuffer cmd = begin_one_time_command_buffer(context, context.frames[0].command_pool);
    begin_upload(staging);

    // debug rays info buffer
    create_buffer(context, sizeof(query_output_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &ray_lines_info);
        VkDescriptorBufferInfo ray_lines_info_info = { ray_lines_info.handle, 0, VK_WHOLE_SIZE };
    for (size_t i = 0; i < context.frames.size(); ++i)
    {
        // set 0
        create_buffer(context, sizeof(camera_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_camera);
        // used as vbo for visualizing
        create_buffer(context, MAX_LIGHTS * sizeof(light_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &frame_resources[i].ubo_light); 
        create_buffer(context, MAX_ENTITIES * sizeof(model_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].ubo_model); 
        create_buffer(context, MAX_ENTITIES * sizeof(material_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].sbo_material);
        create_buffer(context, MAX_ENTITIES * sizeof(mesh_info_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &frame_resources[i].sbo_meshes);

        VkDescriptorBufferInfo ubo_camera_info = { frame_resources[i].ubo_camera.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_light_info  = { frame_resources[i].ubo_light.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_model_info  = { frame_resources[i].ubo_model.handle,  0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo sbo_material_info = { frame_resources[i].sbo_material.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo sbo_meshes_info = { frame_resources[i].sbo_meshes.handle, 0, VK_WHOLE_SIZE };

        create_buffer(context, MAX_LIGHT_TREE_SIZE * sizeof(node_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_light_tree);
        VkDescriptorBufferInfo sbo_light_tree_info = { frame_resources[i].sbo_light_tree.handle, 0, VK_WHOLE_SIZE };

        descriptor_set_t set0(set_layouts[0]);
        bind_buffer(set0, 0, &ubo_camera_info);
        bind_buffer(set0, 1, &ubo_light_info);
        bind_buffer(set0, 2, &ubo_model_info);
        bind_buffer(set0, 3, &sbo_material_info);
        bind_buffer(set0, 4, &sbo_meshes_info);
        bind_buffer(set0, 5, &sbo_light_tree_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set0));

        // set 1
        create_buffer(context, sizeof(scene_info_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_scene);
        //create_image(context, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, context.swapchain.extent.width, context.swapchain.extent.height, 
        //        VkImageUsageFlagBits(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),  VK_IMAGE_ASPECT_COLOR_BIT, 
        //        &frame_resources[i].storage_image);
        //create_sampler(context, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &frame_resources[i].storage_image_sampler);
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
        create_buffer(context, MAX_LIGHTS * sizeof(encoded_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_encoded_lights);
        create_buffer(context, sizeof(light_bounds_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &frame_resources[i].ubo_bounds);

        VkDescriptorBufferInfo sbo_encoded_lights_info = { frame_resources[i].sbo_encoded_lights.handle, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo ubo_bounds_info = { frame_resources[i].ubo_bounds.handle, 0, VK_WHOLE_SIZE };
        descriptor_set_t set3(set_layouts[3]);
        bind_buffer(set3, 0, &ubo_light_info);
        bind_buffer(set3, 1, &sbo_encoded_lights_info);
        bind_buffer(set3, 2, &ubo_bounds_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set3));

        // (sorter)
        descriptor_set_t set4(set_layouts[4]);
        bind_buffer(set4, 0, &sbo_encoded_lights_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set4));

        // (tree builder)
        descriptor_set_t set5(set_layouts[5]);
        bind_buffer(set5, 0, &ubo_light_info);
        bind_buffer(set5, 1, &sbo_encoded_lights_info);
        bind_buffer(set5, 2, &sbo_light_tree_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set5));

        descriptor_set_t set6(set_layouts[2]); // same descriptor layout
        VkDescriptorImageInfo depth_attachment_info = { depth_sampler[i], depth_attachment[i].view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
        bind_image(set6, 0, &depth_attachment_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set6));

        // buffer for bbox lines
        create_buffer(context, MAX_LIGHTS * sizeof(v4) * 12 * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &frame_resources[i].vbo_lines);
        VkDescriptorBufferInfo line_vbo_info = { frame_resources[i].vbo_lines.handle, 0, VK_WHOLE_SIZE };
        descriptor_set_t set7(set_layouts[6]);
        bind_buffer(set7, 0, &sbo_light_tree_info);
        bind_buffer(set7, 1, &line_vbo_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set7));
       
        // buffer for sample ray lines
        create_buffer(context, MAX_LIGHTS_SAMPLED * sizeof(v4) * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &frame_resources[i].vbo_ray_lines);
        VkDescriptorBufferInfo ray_lines_vbo_info = { frame_resources[i].vbo_ray_lines.handle, 0, VK_WHOLE_SIZE };
        create_buffer(context, MAX_LIGHT_TREE_SIZE * sizeof(i32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_nodes_highlight);
        VkDescriptorBufferInfo nodes_highlight_info = { frame_resources[i].sbo_nodes_highlight.handle, 0, VK_WHOLE_SIZE };
        create_buffer(context, MAX_LIGHTS * sizeof(i32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &frame_resources[i].sbo_leaf_select);
        VkDescriptorBufferInfo selected_leafs_info = { frame_resources[i].sbo_leaf_select.handle, 0, VK_WHOLE_SIZE };
        descriptor_set_t set8(set_layouts[7]);
        bind_buffer(set8, 0, &ray_lines_info_info);
        bind_buffer(set8, 1, &ray_lines_vbo_info);
        bind_buffer(set8, 2, &nodes_highlight_info);
        bind_buffer(set8, 3, &selected_leafs_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set8));
       
        // fragment shader for lines pipeline
        descriptor_set_t set9(set_layouts[8]);
        bind_buffer(set9, 0, &nodes_highlight_info);
        frame_resources[i].descriptor_sets.push_back(build_descriptor_set(descriptor_allocator, set9));

        // create sync objects (todo: move this)
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK( vkCreateSemaphore(context.device, &semaphore_info, nullptr, &frame_resources[i].rt_semaphore) );
        VK_CHECK( vkCreateSemaphore(context.device, &semaphore_info, nullptr, &frame_resources[i].prepass_semaphore) );
        VK_CHECK( vkCreateFence(context.device, &fence_info, nullptr, &frame_resources[i].rt_fence) );

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
        VK_CHECK( vkAllocateCommandBuffers(context.device, &alloc, &frame_resources[i].cmd_prepass) );
        VK_CHECK( vkAllocateCommandBuffers(context.device, &alloc, &frame_resources[i].cmd_dwn) );
    }
    end_upload(staging); // uploaded to transfer
    end_and_submit_command_buffer(cmd, context.q_compute);
    VK_CHECK(wait_for_queue(context.q_compute));
    VK_CHECK(wait_for_queue(context.q_transfer));
    vkFreeCommandBuffers(context.device, context.frames[0].command_pool, 1, &cmd); 
}

renderer_t::~renderer_t()
{
    wait_idle(context);
    destroy_imgui(imgui);
    LOG_INFO("Descructor renderer");
    for (auto& f : frame_resources)
    {
        destroy_buffer(context, f.ubo_light);
        destroy_buffer(context, f.ubo_camera);
        destroy_buffer(context, f.ubo_model);
        destroy_buffer(context, f.sbo_material);
        destroy_buffer(context, f.sbo_meshes);
        destroy_buffer(context, f.ubo_scene);
        destroy_buffer(context, f.ubo_bounds);
        destroy_buffer(context, f.sbo_encoded_lights);
        destroy_buffer(context, f.sbo_light_tree);
        destroy_buffer(context, f.vbo_lines);
        destroy_buffer(context, f.sbo_nodes_highlight);
        destroy_buffer(context, f.sbo_leaf_select);
        destroy_buffer(context, f.vbo_ray_lines);

        destroy_image(context, f.storage_image);
        vkDestroySampler(context.device, f.storage_image_sampler, nullptr);

        vkDestroyFence(context.device, f.rt_fence, nullptr);
        vkDestroySemaphore(context.device, f.rt_semaphore, nullptr);
        vkDestroySemaphore(context.device, f.prepass_semaphore, nullptr);
    }

    LOG_INFO("Destroy set layouts");
    for (auto& l : set_layouts)
    {
        destroy_set_layout(context.device, l);
    }
    free(empty_buffer);
    
    destroy_descriptor_allocator(descriptor_allocator);

    for (i32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        destroy_image(context, color_attachment[i]);
        destroy_image(context, depth_attachment[i]);
        vkDestroySampler(context.device, color_sampler[i], nullptr);
        vkDestroySampler(context.device, depth_sampler[i], nullptr);
        vkDestroyFramebuffer(context.device, prepass_framebuffer[i], nullptr);
        vkDestroyFramebuffer(context.device, bbox_framebuffers[i], nullptr);
    }

    vkDestroyRenderPass(context.device, bbox_render_pass, nullptr);
    vkDestroyRenderPass(context.device, prepass_render_pass, nullptr);
    destroy_buffer(context, ray_lines_info);

    LOG_INFO("Destroy pipelines");
    destroy_pipeline(context, &prepass_pipeline);
    destroy_pipeline(context, &debug_pipeline);
    destroy_pipeline(context, &query_pipeline);
    destroy_pipeline(context, &rtx_pipeline);
    destroy_pipeline(context, &post_pipeline);
    destroy_pipeline(context, &points_pipeline);
    destroy_pipeline(context, &lines_pipeline);
    destroy_pipeline(context, &morton_compute_pipeline);
    destroy_pipeline(context, &sort_compute_pipeline);
    destroy_pipeline(context, &tree_leafs_compute_pipeline);
    destroy_pipeline(context, &tree_compute_pipeline);
    destroy_pipeline(context, &bbox_lines_pso);

    destroy_shader_binding_table(context, sbt);
    destroy_shader_binding_table(context, query_sbt);

    destroy_staging_buffer(staging);
    destroy_profiler(profiler);
    destroy_context(context);
}

void renderer_t::draw_scene(scene_t& scene, camera_t& camera, render_state_t& state)
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

        /*
         * Upload all the neceserray data to GPU
         */
        begin_upload(staging);
        if (!mat_uploaded[frame_index])
        {
            // copy materials
            copy_to_buffer(staging, frame_resources[frame_index].sbo_material, scene.materials.size() * sizeof(material_t), (void*)scene.materials.data(), 0);
            // copy mesh_data
            u64 offset = 0;
            for (const auto& mesh : scene.meshes)
            {
                mesh_info_t md;
                md.material_index = -1; // todo
                md.vertex_offset = mesh.vertex_offset;
                md.index_offset = mesh.index_offset;
                copy_to_buffer(staging, frame_resources[frame_index].sbo_meshes, sizeof(mesh_info_t), (void*)&md, offset);
                offset += sizeof(mesh_info_t);
            }
            
            mat_uploaded[frame_index] = true;
        }

        // lights
        copy_to_buffer(staging, frame_resources[frame_index].ubo_light, sizeof(light_t) * scene.lights.size(), (void*)scene.lights.data(), 0);

        /*
         * Calculate the bounding box of the entire cluster of light in the scene
         */
        v3 bbox_min = vec3(FLT_MAX);
        v3 bbox_max = vec3(FLT_MIN);
        for (auto const& light : scene.lights)
        {
            bbox_min = min(bbox_min, light.pos.xyz);
            bbox_max = max(bbox_max, light.pos.xyz);
        }
        light_bounds_t bounds;
        bounds.origin = bbox_min;
        bounds.dims = bbox_max - bbox_min;
        copy_to_buffer(staging, frame_resources[frame_index].ubo_bounds, sizeof(light_bounds_t), (void*)&bounds, 0);

        // upload camera data
        camera_ubo_t camera_data;
        camera_data.pos.xyz = camera.position; 
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

        // clear the ray lines buffer
        {
            if (!empty_buffer)
            {
                empty_buffer = calloc(MAX_LIGHTS * 24, sizeof(v4));
            }
            copy_to_buffer(staging, frame_resources[frame_index].vbo_ray_lines, MAX_LIGHTS_SAMPLED * 2 * sizeof(v4), empty_buffer, 0);
            copy_to_buffer(staging, frame_resources[frame_index].vbo_lines, MAX_LIGHTS * 24 * sizeof(v4), empty_buffer, 0);
            copy_to_buffer(staging, frame_resources[frame_index].sbo_nodes_highlight, MAX_LIGHT_TREE_SIZE * sizeof(i32), empty_buffer, 0);
            copy_to_buffer(staging, frame_resources[frame_index].sbo_leaf_select, MAX_LIGHTS * sizeof(i32), empty_buffer, 0);
        }

        VkSemaphore upload_complete;
        end_upload(staging, &upload_complete);
   
        /*
         * Prepass for albedo and depth buffer.
         * The depth buffer will later be used for combining the result of the ray tracing pipeline and debuggin lines
         */
        { 
            auto cmd = frame_resources[frame_index].cmd_prepass;
            VK_CHECK( begin_command_buffer(cmd) );
            VkClearValue clear[2] = {};
            // attachment order :/
            clear[1].color = {0, 0, 0, 0}; 
            clear[0].depthStencil = {1, 0};

            VkRenderPassBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            begin_info.pNext = nullptr;
            begin_info.renderPass = prepass_render_pass;
            begin_info.framebuffer = prepass_framebuffer[frame_index];
            begin_info.renderArea = { {0,0}, context.swapchain.extent };
            begin_info.clearValueCount = 2;
            begin_info.pClearValues = clear;
            vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, prepass_pipeline.handle);
            u32 first_instance_id = 0;
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &scene.vbo.handle, offsets);
            vkCmdBindIndexBuffer(cmd, scene.ibo.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, prepass_pipeline.layout, 0,
                    1, &frame_resources[frame_index].descriptor_sets[0], 0, nullptr);

            for (auto const& batch : batches)
            {
                auto& mesh = meshes[batch.mesh_id];
                vkCmdDrawIndexed(cmd, mesh.index_count, batch.instance_count, 
                        mesh.index_offset, mesh.vertex_offset, first_instance_id);
                first_instance_id += batch.instance_count;
            }
            vkCmdEndRenderPass(cmd);
            VK_CHECK( vkEndCommandBuffer(cmd) );
            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            submit_info.waitSemaphoreCount = 0;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &cmd;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &frame_resources[frame_index].prepass_semaphore;

            VK_CHECK( vkQueueSubmit(context.q_graphics, 1, &submit_info, nullptr) );
        }

        VkCommandBuffer cmd = frame->command_buffer;
        VK_CHECK( begin_command_buffer(cmd) );
        begin_timer(profiler, cmd);
        i32 num_lights = static_cast<i32>(scene.lights.size());
        i32 num_leaf_nodes = num_lights; // will be modified at bitonic sort

        // morton encoding
        if (ENABLE_MORTON_ENCODE) 
        {
            CHECKPOINT(cmd, "[PRE] MORTON ENCODING");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, morton_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, morton_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[3], 0, nullptr);
            vkCmdPushConstants(cmd, morton_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(i32), &num_lights);
            u32 threads = MAX(static_cast<u32>(num_lights)/512, 1);
            vkCmdDispatch(cmd, threads, 1, 1);

            // memory barrier to wait for finish morton encoding
            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            CHECKPOINT(cmd, "[POST] MORTON ENCODING");
        }

        // bitonic sort lights
        if (ENABLE_SORT_LIGHTS) 
        {
            CHECKPOINT(cmd, "[PRE] BITONIC SORT");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sort_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sort_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[4], 0, nullptr);

            // add dummy nodes to get to power of 2
            num_leaf_nodes = next_pow2(num_lights);
            struct 
            {
                int j;
                int k;
                int num_lights;
                int total_nodes;
            } constants;
    
            u32 threads = MAX(num_leaf_nodes/512, 1);
            constants.num_lights  = num_lights;
            constants.total_nodes = num_leaf_nodes;
            for (int k = 2; k <= num_leaf_nodes; k <<= 1) 
            {
                for (int j = k >> 1; j > 0; j >>= 1)
                {
                    constants.j = j;
                    constants.k = k;
                    //LOG_INFO("k = %d, j = %d, threads = %d", k, j, threads);
                    vkCmdPushConstants(cmd, sort_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
                    vkCmdDispatch(cmd, threads, 1, 1);

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
            CHECKPOINT(cmd, "LIGHT TREE LEAF NODES");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_leafs_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_leafs_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[5], 0, nullptr);
            u32 leaf_nodes = static_cast<u32>(num_leaf_nodes);
            vkCmdPushConstants(cmd, tree_leafs_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &leaf_nodes);
            u32 threads = MAX(1, leaf_nodes/512);
            vkCmdDispatch(cmd, threads, 1, 1);

            CHECKPOINT(cmd, "[PRE] LIGHT TREE BUILD");
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_compute_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tree_compute_pipeline.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[5], 0, nullptr);

            // bottom up
            // each level waits for previous level to be processed and every node in a dispatch is computed in parrallel
            // this is better than creating all levels at once and merging from the leaf nodes directly because of cache 
            //  (nodes at the top are at the end of the array while leaf nodes are at the start)
            u32 h = static_cast<u32>(log2(num_leaf_nodes));
            u32 src_lvl = 0;
            for (u32 dst_lvl = 1; dst_lvl <= h; dst_lvl++)
            {
                struct 
                {
                    u32 height;        // height of the tree (actually corresponds to 2^h = leaf nodes)
                    u32 total_nodes;   // total nodes being processed in the current dispatch
                    u32 start_id;      // used to calculate id and the level of a node being processed (root = 0)
                    u32 src_level;     // the level from which to create nodes from (merge nodes together)
                    u32 start_src_id;  // the start id for the src level nodes (used to access them in the array)
                } constants;

                // todo: could group level 0 -> 9 together because L1 is 48kb (NVIDIA 1060 6GB)
                // 0 -> 9 = 2^10 - 1 = 1023 < 1536 nodes (48kb/32b = 1536)
    
                constants.height       = h;
                constants.total_nodes  = (1 << (h - src_lvl)) - (1 << (h - dst_lvl));
                constants.start_src_id = (1 << (h + 1)) - (1 << (h - src_lvl + 1));
                constants.start_id     = ((1 << (h - src_lvl + 1)) - 1) - (1 << (h - src_lvl)) - constants.total_nodes;
                constants.src_level    = h - src_lvl;
                //LOG_INFO("level %d to %d, nodes = %d, start_srd_id=%d, src_level=%d", h - src_lvl, h - dst_lvl, 
                //            constants.total_nodes, constants.start_src_id, constants.src_level);
                
                // wait for previous level to finish
                VkMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                        0, 1, &barrier, 0, nullptr, 0, nullptr);

                vkCmdPushConstants(cmd, tree_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
                u32 groups = MAX(constants.total_nodes / 512, 1);
                vkCmdDispatch(cmd, groups, 1, 1);
                
                src_lvl = dst_lvl;
            }

            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
            CHECKPOINT(cmd, "[POST] LIGHT TREE BUILD");
        }

        if (ENABLE_BBOX_DEBUG) 
        {
            // write lines to vbo for debuging
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bbox_lines_pso.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bbox_lines_pso.layout, 0, 
                    1, &frame_resources[frame_index].descriptor_sets[7], 0, nullptr);
            struct {
                i32 total_nodes;
                i32 offset;
            } constants;
            i32 h = static_cast<i32>(log2(num_leaf_nodes));
            constants.total_nodes = (1 << h) - 1;
            constants.offset = num_leaf_nodes;
            vkCmdPushConstants(cmd, bbox_lines_pso.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
            u32 threads = MAX(static_cast<u32>(constants.total_nodes)/512, 1);
            vkCmdDispatch(cmd, threads, 1, 1);
            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                    0, 1, &barrier, 0, nullptr, 0, nullptr);
        }

        // render using light tree
        if (ENABLE_RTX)
        {
            CHECKPOINT(cmd, "[PRE] RAYTRACING");
            // ray query to get intersection point 
            if (state.render_sample_lines && is_key_pressed(*window, KEY_R))
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, query_pipeline.handle);
                VkDescriptorSet sets[3] = { 
                    frame_resources[frame_index].descriptor_sets[0],
                    frame_resources[frame_index].descriptor_sets[1],
                    frame_resources[frame_index].descriptor_sets[8],
                };
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, query_pipeline.layout, 0, 3, sets, 0, nullptr);

                struct 
                {
                    v2 screen_uv;
                    v2 extent;
                    i32 is_ortho; // boolean
                } constants;
                
                constants.extent = vec2(context.swapchain.extent.width, context.swapchain.extent.height);
                constants.screen_uv = floor(constants.extent * state.screen_uv);
                constants.is_ortho = static_cast<i32>(camera.is_ortho);
                vkCmdPushConstants(cmd, query_pipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(constants), &constants);
                vkCmdTraceRays(cmd, &query_sbt.rgen, &query_sbt.miss, &query_sbt.hit, &query_sbt.call, 1, 1, 1); // 1 ray

                // wait for ray query
                VkMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            }

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx_pipeline.handle);
            VkDescriptorSet sets[3] = { 
                frame_resources[frame_index].descriptor_sets[0],
                frame_resources[frame_index].descriptor_sets[1],
                frame_resources[frame_index].descriptor_sets[8],
            };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx_pipeline.layout, 0, 3, sets, 0, nullptr);

            struct 
            {
                i32 num_nodes;
                i32 num_leaf_nodes;
                f32 time;
                u32 num_samples;
                u32 cut_size;
                i32 is_ortho; // boolean
            } constants;
            
            u32 h = static_cast<u32>(log2(num_leaf_nodes));
            timespec tp;
            clock_gettime(CLOCK_REALTIME, &tp);
            constants.num_nodes = ((1 << (h + 1)) - 1);
            constants.num_leaf_nodes = num_leaf_nodes;
            constants.num_samples = state.num_samples;
            constants.cut_size = state.cut_size;
            constants.time = (float)tp.tv_nsec;
            constants.is_ortho = static_cast<i32>(camera.is_ortho);
            vkCmdPushConstants(cmd, rtx_pipeline.layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(constants), &constants);
            vkCmdTraceRays(cmd, &sbt.rgen, &sbt.miss, &sbt.hit, &sbt.call, context.swapchain.extent.width, context.swapchain.extent.height, 1);
            CHECKPOINT(cmd, "[POST] RAYTRACING");
        }

        end_timer(profiler,cmd);
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

        /* 
         * Copy the debugging info into a local buffer and update the render state
         * such that imgui can display this information
         */
        if (ENABLE_VERIFY)
        {
            //LOG_INFO("COMPUTE TIME %lf", get_results(profiler));
            // compute to local
            cmd = frame_resources[frame_index].cmd_dwn;
            VK_CHECK( begin_command_buffer(cmd) ); 
            u32 h = static_cast<u32>(log2(num_leaf_nodes));
            u32 node_count = ((1 << (h + 1)) - 1);

            u32 size_light_tree = sizeof(node_t) * node_count;
            u32 size_selected_nodes = sizeof(i32) * node_count;
            u32 size_selected_leafs = sizeof(i32) * num_leaf_nodes;
            u32 size = size_light_tree + size_selected_nodes + size_selected_leafs;

            buffer_t local;
            create_buffer(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &local,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            VkBufferCopy copy_tree = {};
            copy_tree.size = size_light_tree;
            vkCmdCopyBuffer(cmd, frame_resources[frame_index].sbo_light_tree.handle, local.handle, 1, &copy_tree);

            VkBufferCopy copy_nodes = {};
            copy_nodes.dstOffset = size_light_tree;
            copy_nodes.size = size_selected_nodes;
            vkCmdCopyBuffer(cmd, frame_resources[frame_index].sbo_nodes_highlight.handle, local.handle, 1, &copy_nodes);
            
            VkBufferCopy copy_leafs = {};
            copy_leafs.dstOffset = size_light_tree + size_selected_nodes;
            copy_leafs.size = size_selected_leafs;
            vkCmdCopyBuffer(cmd, frame_resources[frame_index].sbo_leaf_select.handle, local.handle, 1, &copy_leafs);

            VK_CHECK( vkEndCommandBuffer(cmd) );

            VkPipelineStageFlags _dst_wait_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &frame_resources[frame_index].rt_semaphore;
            submit_info.pWaitDstStageMask = &_dst_wait_mask;
            submit_info.signalSemaphoreCount = 0;
            submit_info.pSignalSemaphores = &frame->render_semaphore;
            submit_info.pCommandBuffers = &cmd;
            VK_CHECK( vkQueueSubmit(context.q_transfer, 1, &submit_info, nullptr) ); 
            VK_CHECK( wait_for_queue(context.q_transfer) );

            u8* p_data;
            context.allocator.map_memory(local.allocation, (void**)&p_data);

            // write node ids
            {
                auto* ptr = reinterpret_cast<node_t*>(p_data);
                for (size_t i = 0; i < node_count; i++)
                {
                    state.cut[i].id = ptr[i].id;
                }
            }

            // write selected nodes
            { 
                auto* ptr = reinterpret_cast<i32*>(p_data + size_light_tree);
                for (i32 i = 0; i < node_count; i++)
                {
                    state.cut[i].selected = ptr[i];
                }
            }

            // write selected leafs
            {
                auto* ptr = reinterpret_cast<i32*>(p_data + size_light_tree + size_selected_nodes);
                memcpy(&state.selected_leafs, ptr, size_selected_leafs);
            }

            context.allocator.unmap_memory(local.allocation);
            destroy_buffer(context, local);
        }

        cmd = frame_resources[frame_index].cmd;
        VK_CHECK( begin_command_buffer(cmd) ); 
        
        VkClearValue clear[2];
        clear[0].color = {0, 0, 0, 0};
        clear[1].depthStencil = {1, 0};

        VkRenderPassBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.renderPass = bbox_render_pass;
        begin_info.framebuffer = bbox_framebuffers[frame_index];
        begin_info.renderArea = { {0,0}, context.swapchain.extent };
        begin_info.clearValueCount = 2;
        begin_info.pClearValues = clear;
        vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

        VkDeviceSize offsets[1] = {0};
        if (state.render_bboxes || state.render_sample_lines)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.handle);
            VkDescriptorSet sets[2] = { 
                frame_resources[frame_index].descriptor_sets[0],
                frame_resources[frame_index].descriptor_sets[9],
            };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.layout, 0, 2, sets, 0, nullptr);
        }

        struct {
            v3 color;
            i32 is_bbox; // bool
            v3 highlight;
            u32 offset;
            i32 only_selected; // bool
        } constants;
        
        constants.color = vec3(1);
        constants.is_bbox = 1;
        constants.only_selected = static_cast<i32>(state.render_only_selected_nodes);
        constants.highlight = vec3(1,0,0);
        constants.offset = num_leaf_nodes;

        if (state.render_bboxes)
        {
            vkCmdBindVertexBuffers(cmd, 0, 1, &frame_resources[frame_index].vbo_lines.handle, offsets);
            vkCmdPushConstants(cmd, lines_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            u32 h = static_cast<u32>(log2(num_leaf_nodes));
            u32 vertex_count = 24 * ((1 << h) - 1);
            if (state.render_step_mode)
            {
                u32 _h = h == 0 ? 0 : h - 1;
                u32 s = MIN(state.step, h);
                u32 l = 0;
                for (u32 i = 0; i < s; i++)
                {
                    l |= (1 << (_h - i));
                }
                vertex_count = 24 * l;
            }
            vkCmdDraw(cmd, vertex_count, 1, 0, 0);
        }

        // draw ray lines from sampled here
        if (state.render_sample_lines)
        {
            vkCmdBindVertexBuffers(cmd, 0, 1, &frame_resources[frame_index].vbo_ray_lines.handle, offsets);
            constants.color = vec3(1, 0, 0);
            constants.is_bbox = 0;
            vkCmdPushConstants(cmd, lines_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            vkCmdDraw(cmd, state.cut_size * 2, 1, 0, 0);
        }

        // draw light points
        if (state.render_lights)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, points_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, points_pipeline.layout, 0,
                    1, &frame_resources[frame_index].descriptor_sets[0], 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, &frame_resources[frame_index].ubo_light.handle, offsets);
            vkCmdDraw(cmd, num_lights, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd);

        /*
         * Post process (write to quad)
         * Or if debug is enabled: see depth buffer
         */
        begin_render_pass(context, cmd, clear, 2); 
        if (!state.render_depth_buffer)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline.layout, 0,
                    1, &frame_resources[frame_index].descriptor_sets[2], 0, nullptr);
        } 
        else 
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_pipeline.layout, 0,
                    1, &frame_resources[frame_index].descriptor_sets[6], 0, nullptr);
            struct {
                f32 znear;
                f32 zfar;
            } constants;
            constants.znear = camera.znear;
            constants.zfar = camera.zfar;
            vkCmdPushConstants(cmd, debug_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
        }
        vkCmdDraw(cmd, 4, 1, 0, 0);

        // draw gui
        render_imgui(cmd, imgui);
        vkCmdEndRenderPass(cmd);
        VK_CHECK( vkEndCommandBuffer(cmd) );

        VkPipelineStageFlags _dst_wait_mask[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
        VkSemaphore wait_semaphores[2] = {frame_resources[frame_index].rt_semaphore, frame_resources[frame_index].prepass_semaphore};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &wait_semaphores[1];
        submit_info.pWaitDstStageMask = &_dst_wait_mask[1];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &frame->render_semaphore;
        submit_info.pCommandBuffers = &cmd;

        VK_CHECK( vkQueueSubmit(context.q_graphics, 1, &submit_info, frame->fence) ); 
    }

    //graphics_submit_frame(context, frame);
    present_frame(context, frame);
}



