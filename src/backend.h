#ifndef BACKEND_H
#define BACKEND_H

#include "common.h"
#include "allocator.h"
#include "log.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

#define ONE_SECOND_IN_NANOSECONDS 1000000000

//#define VK_DEBUG_CHECKPOINT_NV
#define VK_RTX_ON

struct window_t;

extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelines; 
extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructure;
extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructure;
extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddress;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandles;
extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructures;
extern PFN_vkCmdTraceRaysKHR vkCmdTraceRays;

#ifdef VK_DEBUG_CHECKPOINT_NV
extern PFN_vkCmdSetCheckpointNV vkCmdSetCheckpoint;
extern PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointData;
#endif

#ifndef VK_CHECK
#define VK_CHECK(x) { \
	if ((VkResult) x != VK_SUCCESS) \
	{ \
        LOG_ERROR("VkResult error code %d", x); \
        std::abort();\
	} \
} 
#endif

#define BUFFERED_FRAMES 2

struct buffer_t
{
	VkBuffer     handle;
	allocation_t allocation;
};

struct image_t
{
	VkImage      handle;
	VkImageView  view;
	allocation_t allocation;
};

struct texture_t
{
	VkImage        image;
	VkImageView    view;
	VkSampler      sampler;
	allocation_t   allocation;
};

struct frame_t
{
	VkFence         fence;
	VkSemaphore     render_semaphore;
	VkSemaphore     present_semaphore;
	VkCommandPool   command_pool;
	VkCommandBuffer command_buffer;
	std::vector<VkDescriptorSet> descriptor_sets;
};

struct swapchain_t
{
	VkSwapchainKHR           handle;
	VkFormat                 format;
	VkExtent2D               extent;
	std::vector<VkImage>     images;
	std::vector<VkImageView> image_views;
    u32                      image_index;
};

struct gpu_context_t
{
	VkInstance                 instance;
	VkPhysicalDevice           physical_device;
	VkDevice                   device;
	VkSurfaceKHR               surface;
	vk_allocator_t             allocator;

	u32                        q_graphics_index;
    u32                        q_compute_index;
	u32                        q_present_index;
	u32                        q_transfer_index;
	VkQueue                    q_graphics;
    VkQueue                    q_compute;
	VkQueue                    q_present;
	VkQueue                    q_transfer;

	swapchain_t                swapchain; 
	image_t                    depth_buffer;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<frame_t>       frames;
    u32                        frame_index;

	VkRenderPass               render_pass;
	VkDescriptorPool           descriptor_pool; // per frame ?

	VkPhysicalDeviceProperties                      device_properties;
	VkPhysicalDeviceMemoryProperties                memory_properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_properties;
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT   debug_messenger;
#endif
};


void init_context(gpu_context_t& ctx, window_t* window);
void destroy_context(gpu_context_t& ctx);
void create_swapchain(gpu_context_t& ctx, window_t* window);
void create_render_pass(gpu_context_t& ctx, VkRenderPass* render_pass);

VkResult create_descriptor_set_layout(gpu_context_t& ctx, u32 bind_count, VkDescriptorSetLayoutBinding *p_bindings, VkDescriptorSetLayout* layout);
bool create_texture(gpu_context_t& ctx, u32 width, u32 height, texture_t* p_texture);
bool create_image(gpu_context_t& ctx, VkImageType type, VkFormat format, u32 width, u32 height, VkImageUsageFlagBits usage, VkImageAspectFlags aspect_mask, image_t* p_image);
void destroy_image(gpu_context_t& ctx, image_t& image);
bool create_sampler(gpu_context_t& ctx, VkFilter filter, VkSamplerAddressMode address_mode, VkSampler* p_sampler);

bool create_buffer(gpu_context_t& ctx, u32 size, u32 usage, buffer_t* p_buffer, 
        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
void write_buffer(gpu_context_t& ctx, buffer_t& buffer, u32 size, void* p_data);
void destroy_buffer(gpu_context_t& ctx, buffer_t& buffer);
VkDeviceAddress get_buffer_device_address(gpu_context_t& ctx, VkBuffer buffer);

VkCommandBuffer begin_one_time_command_buffer(gpu_context_t& ctx, VkCommandPool pool);
void end_and_submit_command_buffer(VkCommandBuffer cmd, VkQueue queue, VkSemaphore* wait_semaphore = nullptr, 
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

VkResult begin_command_buffer(VkCommandBuffer cmd);
void begin_render_pass(gpu_context_t& ctx, VkCommandBuffer cmd, VkClearValue *clear, u32 clear_count); 

void prepare_frame(gpu_context_t&, i32* frame_index, frame_t** frame);
void get_next_swapchain_image(gpu_context_t& ctx, frame_t* frame);
void graphics_submit_frame(gpu_context_t& ctx, frame_t* frame);
void present_frame(gpu_context_t& ctx, frame_t* frame);

void on_window_resize(gpu_context_t& ctx, window_t& window);
inline void wait_idle(gpu_context_t& ctx) { vkDeviceWaitIdle(ctx.device); }
inline VkResult wait_for_queue(VkQueue queue) { return vkQueueWaitIdle(queue); }
inline VkImage get_swapchain_image(gpu_context_t& ctx) { return ctx.swapchain.images[ctx.swapchain.image_index]; };

#endif // BACKEND_H
