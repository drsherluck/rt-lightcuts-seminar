#include "backend.h"
#include "window.h"
#include "file.h"
#include "math.h"

#include <algorithm>
#include <set>
#include <cstring>
#include <cassert>


PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelines;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructure;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructure;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddress;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandles;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructures;
PFN_vkCmdTraceRaysKHR vkCmdTraceRays;

#ifdef VK_DEBUG_CHECKPOINT_NV
PFN_vkCmdSetCheckpointNV vkCmdSetCheckpoint;
PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointData;
#endif

static void load_extension_functions(VkDevice device)
{
#ifdef VK_RTX_ON
    vkCreateRayTracingPipelines = (PFN_vkCreateRayTracingPipelinesKHR) vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    vkCreateAccelerationStructure = (PFN_vkCreateAccelerationStructureKHR) vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    vkDestroyAccelerationStructure = (PFN_vkDestroyAccelerationStructureKHR) vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    vkGetAccelerationStructureBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR) vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelerationStructureDeviceAddress = (PFN_vkGetAccelerationStructureDeviceAddressKHR) vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    vkGetRayTracingShaderGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR) vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    vkCmdBuildAccelerationStructures = (PFN_vkCmdBuildAccelerationStructuresKHR) vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    vkCmdTraceRays = (PFN_vkCmdTraceRaysKHR) vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

    assert(vkCreateRayTracingPipelines && vkCreateAccelerationStructure && vkDestroyAccelerationStructure
            && vkGetAccelerationStructureBuildSizes && vkGetAccelerationStructureDeviceAddress && vkGetRayTracingShaderGroupHandles 
            && vkCmdBuildAccelerationStructures && vkCmdTraceRays);
#endif

#ifdef VK_DEBUG_CHECKPOINT_NV
    vkCmdSetCheckpoint = (PFN_vkCmdSetCheckpointNV) vkGetDeviceProcAddr(device, "vkCmdSetCheckpointNV");
    vkGetQueueCheckpointData = (PFN_vkGetQueueCheckpointDataNV) vkGetDeviceProcAddr(device, "vkGetQueueCheckpointDataNV");

    assert(vkCmdSetCheckpoint && vkGetQueueCheckpointData);
#endif
}


#ifdef _DEBUG
static const std::vector<const char*> validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
{
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
				messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) 
		{
			LOG_ERROR(pCallbackData->pMessage);
		}
		return VK_FALSE;
}

static void destroy_debug_utils_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, messenger, pAllocator);
	}
}
#endif

static const std::vector<const char*> device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef VK_RTX_ON
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
//  VK_KHR_RAY_QUERY_EXTENSION_NAME (not support on NVIDIA GeForce GTX 1060 6GB)
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
#endif
#ifdef VK_DEBUG_CHECKPOINT_NV
    VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
#endif
};

static void create_render_pass(gpu_context_t& ctx);
static void create_framebuffers(gpu_context_t& ctx);
static void create_frame_resources(gpu_context_t& ctx);
static void create_depth_buffer(gpu_context_t& ctx);
static bool create_descriptor_pool(gpu_context_t& ctx);

void init_context(gpu_context_t& ctx, window_t* window)
{
	// Create instance
	u32 count; 
	std::vector<const char*> extensions = window->get_required_vulkan_extensions();

#ifdef _DEBUG
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> available(count);
	vkEnumerateInstanceLayerProperties(&count, available.data());

	bool found;
	for (const char* name : validation_layers)
	{
		found = false;
		for (const auto& layer : available)
		{
			if (strcmp(name, layer.layerName) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found) break;
	}
	if (!found)
	{
		throw std::runtime_error("Validation layers incomplete");
	}
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	VkApplicationInfo app_info{};
	app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext              = nullptr;
	app_info.pApplicationName   = "vk_app";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName        = "vk_engine";
	app_info.apiVersion         = VK_API_VERSION_1_2;

	VkInstanceCreateInfo create_info{};
	create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo        = &app_info;
	create_info.enabledExtensionCount   = static_cast<u32>(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();
	create_info.enabledLayerCount       = 0;
#ifdef _DEBUG
	create_info.enabledLayerCount       = static_cast<u32>(validation_layers.size());
	create_info.ppEnabledLayerNames     = validation_layers.data();
#endif

	VK_CHECK( vkCreateInstance(&create_info, nullptr, &ctx.instance) );
	LOG_INFO("Vulkan instance created");

#ifdef _DEBUG
	VkDebugUtilsMessengerCreateInfoEXT debug{};
	debug.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug.pfnUserCallback = debug_callback;
	debug.pUserData       = nullptr;
	debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug.messageType    = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;


	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(ctx.instance, "vkCreateDebugUtilsMessengerEXT");
	if (func == nullptr)
	{
		std::runtime_error("Failed to set up debug messenger");
	}
	VK_CHECK( func(ctx.instance, &debug, nullptr, &ctx.debug_messenger) );
#endif

	// Create surface
	VK_CHECK( window->create_surface(ctx.instance, &ctx.surface) );
	LOG_INFO("Surface created");

	// Pick physical device
	vkEnumeratePhysicalDevices(ctx.instance, &count, nullptr);

	if (count == 0)
	{
		throw std::runtime_error("Failed to find a physical device.");
	}

	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(ctx.instance, &count, devices.data());

	LOG_INFO("Trying %d physical devices", count);

	ctx.physical_device = VK_NULL_HANDLE;
	for (auto const& device : devices)
	{
		// Check if device supports required device extensions
		u32 count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
		std::vector<VkExtensionProperties> available(count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

        VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(device, &device_properties);
        LOG_INFO("Device name: %s", device_properties.deviceName);

		bool found;
		for (auto const& e : device_extensions)
		{
			found = false;
			for (auto const& p : available)
			{
				if (strcmp(p.extensionName, e) == 0)
				{
					found = true;
					break;
				}
			}
			
			if (!found) 
            {
                LOG_ERROR("Device extension not found: %s", e);
                break;
            }
		}

		if (!found)
		{
			continue;
		}

		// get device properties and ray tracing properties
        ctx.rt_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        ctx.rt_properties.pNext = nullptr;

        VkPhysicalDeviceProperties2 device_properties_2;
        device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device_properties_2.pNext = &ctx.rt_properties;
		vkGetPhysicalDeviceProperties2(device, &device_properties_2);
        // should just move
        ctx.device_properties = device_properties_2.properties;

		// Check for queues
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
		std::vector<VkQueueFamilyProperties> properties(count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
		ctx.q_graphics_index = ctx.q_present_index = ctx.q_transfer_index = ctx.q_compute_index = count;

        // TODO Choose seperate queue for compute and transfer if possible
		// Get queue indices
		for (u32 i = 0; i < count; ++i)
		{
			VkBool32 present_support = VK_FALSE;
			VkQueueFamilyProperties& queue_family = properties[i];
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, ctx.surface, &present_support);

			if (queue_family.queueCount <= 0 )
				continue;

			if (ctx.q_graphics_index >= count && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				ctx.q_graphics_index = i;
			}
            if (ctx.q_compute_index >= count && queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                ctx.q_compute_index = i;
            }
			if (ctx.q_present_index >= count && present_support)
			{
				ctx.q_present_index = i;
			}
			if (ctx.q_transfer_index >= count && queue_family.queueFlags  & VK_QUEUE_TRANSFER_BIT)
			{
				ctx.q_transfer_index = i;
			}
		}

		LOG_INFO("Present: %d, Graphics: %d, Compute: %d, Transfer: %d", 
                ctx.q_present_index, ctx.q_graphics_index, ctx.q_compute_index, ctx.q_transfer_index);

		if (ctx.q_present_index < count && ctx.q_graphics_index < count && ctx.q_transfer_index < count && ctx.q_compute_index < count)
		{
			ctx.physical_device = device;
			vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &ctx.memory_properties);
			LOG_INFO("Physical device selected");
			break;
		}
	}

	if (ctx.physical_device == VK_NULL_HANDLE) 
	{
		throw std::runtime_error("No suitable physical device found.");
	}

	// Create queues
	std::set<u32> indices = { ctx.q_graphics_index, ctx.q_present_index, ctx.q_transfer_index, ctx.q_compute_index };
	std::vector<VkDeviceQueueCreateInfo> queue_infos;
	f32 priority = 1.0f;

	for (auto const& i : indices)
	{
		VkDeviceQueueCreateInfo info{};
		info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.flags            = 0;
		info.queueFamilyIndex = i;
		info.queueCount       = 1;
		info.pQueuePriorities = &priority;
		queue_infos.push_back(info);
	}
	
	// device features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features{};
    ray_tracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    ray_tracing_features.pNext = nullptr;
    ray_tracing_features.rayTracingPipeline = VK_TRUE;
    ray_tracing_features.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
    ray_tracing_features.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
    ray_tracing_features.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
    ray_tracing_features.rayTraversalPrimitiveCulling = VK_FALSE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
    acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration_structure_features.pNext = &ray_tracing_features;
    acceleration_structure_features.accelerationStructure = VK_TRUE;
    acceleration_structure_features.accelerationStructureCaptureReplay = VK_FALSE;
    acceleration_structure_features.accelerationStructureIndirectBuild = VK_FALSE;
    acceleration_structure_features.accelerationStructureHostCommands = VK_FALSE;
    acceleration_structure_features.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address_features{};
    buffer_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    buffer_address_features.pNext = &acceleration_structure_features;
    buffer_address_features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &buffer_address_features;
    features.features.shaderInt64 = VK_TRUE;
	//features.samplerAnisotropy = VK_TRUE;
    
	// logical device 
	VkDeviceCreateInfo device_info{};
	device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext                   = &features;
	device_info.enabledLayerCount       = 0;
	device_info.enabledExtensionCount   = static_cast<u32>(device_extensions.size());
	device_info.ppEnabledExtensionNames = device_extensions.data();
	device_info.pEnabledFeatures        = nullptr; // required if using pNext VkPhysicalDeviceFeatures2
	device_info.queueCreateInfoCount    = static_cast<u32>(queue_infos.size());
	device_info.pQueueCreateInfos       = queue_infos.data();

	VK_CHECK( vkCreateDevice(ctx.physical_device, &device_info, nullptr, &ctx.device) );
	LOG_INFO("Logical device created");

    load_extension_functions(ctx.device);
	
	// init allocator
	ctx.allocator.init_allocator(ctx.device, ctx.physical_device, ctx.instance);
	LOG_INFO("Gpu memory allocator initialized");

	// create queues
	vkGetDeviceQueue(ctx.device, ctx.q_graphics_index, 0, &ctx.q_graphics);
	vkGetDeviceQueue(ctx.device, ctx.q_present_index, 0, &ctx.q_present);
	vkGetDeviceQueue(ctx.device, ctx.q_transfer_index, 0, &ctx.q_transfer);
    vkGetDeviceQueue(ctx.device, ctx.q_compute_index, 0, &ctx.q_compute);
	LOG_INFO("Device queues created");

	// set swapchain to null to avoid errors when creating swapchain
	ctx.swapchain.handle = VK_NULL_HANDLE;
	create_swapchain(ctx, window);
    create_depth_buffer(ctx);
	create_render_pass(ctx, &ctx.render_pass);
	create_framebuffers(ctx);
	create_frame_resources(ctx);
}

static void create_frame_resources(gpu_context_t& ctx)
{
	// Create fences
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// Create semaphores
	VkSemaphoreCreateInfo sempahore_info{};
	sempahore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	sempahore_info.pNext = nullptr;

	// Create command pools
	VkCommandPoolCreateInfo cmd_pool_info{};
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.queueFamilyIndex = ctx.q_graphics_index;
#ifdef  VK_RTX_ON
	cmd_pool_info.queueFamilyIndex = ctx.q_compute_index;
#endif

	ctx.frames.resize(BUFFERED_FRAMES);
	for (size_t i = 0; i < BUFFERED_FRAMES; ++i)
	{
		VK_CHECK( vkCreateSemaphore(ctx.device, &sempahore_info, nullptr, &ctx.frames[i].render_semaphore) );
		VK_CHECK( vkCreateSemaphore(ctx.device, &sempahore_info, nullptr, &ctx.frames[i].present_semaphore) );
		VK_CHECK( vkCreateFence(ctx.device, &fence_info, nullptr, &ctx.frames[i].fence) );
		VK_CHECK( vkCreateCommandPool(ctx.device, &cmd_pool_info, nullptr, &ctx.frames[i].command_pool) );

		// Create command buffer
		VkCommandBufferAllocateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		buffer_info.commandPool = ctx.frames[i].command_pool;
		buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		buffer_info.commandBufferCount = 1;

		VK_CHECK( vkAllocateCommandBuffers(ctx.device, &buffer_info, &ctx.frames[i].command_buffer) );
	}
	LOG_INFO("Frame resources created");
}

void create_swapchain(gpu_context_t& ctx, window_t* window)
{
	if (ctx.device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(ctx.device);
	}
	
	// might need to create empty ctx first with VK_NULL_HANDLES so we don't get errors
	VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &capabilities);

	u32 support_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &support_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(support_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &support_count, formats.data()); 

	VkSurfaceFormatKHR selected_format = formats[0];
	for (auto const& f : formats)
	{
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			selected_format = f;
			break;
		}
	}

	u32 present_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(present_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_count, present_modes.data());

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (auto const& p : present_modes)
	{
		if (p == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = p;
			break;
		}
	}
	
	u32 image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < image_count)
	{
		image_count = capabilities.maxImageCount;
	}

	VkExtent2D extent = capabilities.currentExtent;
	if (extent.width == UINT32_MAX)
	{
		extent.width = std::clamp((u32) window->get_width(), capabilities.minImageExtent.width, 
				capabilities.maxImageExtent.width);
		extent.height = std::clamp((u32) window->get_height(), capabilities.minImageExtent.height, 
				capabilities.maxImageExtent.height);
	}
	LOG_INFO("Resolution: %dx%d", extent.width, extent.height);

	VkSwapchainCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.pNext = nullptr;
	info.surface = ctx.surface;
	info.minImageCount = image_count;
	info.imageFormat = selected_format.format;
	info.imageColorSpace = selected_format.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Set here VK_IMAGE_USAGE_STORAGE_BIT
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = capabilities.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = present_mode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = old_swapchain;

	// special case when graphics queue is the same as the present queue
	if (ctx.q_graphics_index != ctx.q_present_index)
	{
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount = 2;
		u32 indices[2] = { ctx.q_graphics_index, ctx.q_present_index };
		info.pQueueFamilyIndices = indices;
	}

	// set format, extent and handle
	auto& swapchain = ctx.swapchain;
	swapchain.format = selected_format.format;
	swapchain.extent = extent;
	VK_CHECK( vkCreateSwapchainKHR(ctx.device, &info, nullptr, &swapchain.handle) );
	LOG_INFO("Swapchain created");

	// get images
	vkGetSwapchainImagesKHR(ctx.device, swapchain.handle, &image_count, nullptr);
	swapchain.images.resize(image_count);
	vkGetSwapchainImagesKHR(ctx.device, swapchain.handle, &image_count, swapchain.images.data());

    LOG_INFO("%d swapchain images", image_count);

	// create image views
	swapchain.image_views.resize(image_count);
	for (size_t i = 0; i < image_count; ++i)
	{
		VkImageViewCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;
		info.image = swapchain.images[i];
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = selected_format.format;
		info.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		};
		info.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1
		};

		VK_CHECK( vkCreateImageView(ctx.device, &info, nullptr, &swapchain.image_views[i]) );
	}

	if (old_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(ctx.device, old_swapchain, nullptr);
		LOG_INFO("Old swapchain destroyed");
	}
}

void create_render_pass(gpu_context_t& ctx, VkRenderPass* render_pass)
{
	VkAttachmentDescription color_info{};
	color_info.flags          = 0;
	color_info.format         = ctx.swapchain.format;
	color_info.samples        = VK_SAMPLE_COUNT_1_BIT;
	color_info.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_info.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	color_info.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_info.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_info.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	color_info.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depth_info{};
	depth_info.flags          = 0;
	depth_info.format         = VK_FORMAT_D32_SFLOAT; // hardcoded
	depth_info.samples        = VK_SAMPLE_COUNT_1_BIT;
	depth_info.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_info.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_info.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_info.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_info.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_info.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription attachments[] = { color_info, depth_info };
	
	// references
	VkAttachmentReference color_attachment_refs[] = {
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};
	VkAttachmentReference depth_attachment_ref =  {
		1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass_info{};
	subpass_info.flags                   = 0;
	subpass_info.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_info.inputAttachmentCount    = 0;
	subpass_info.pInputAttachments       = nullptr;
	subpass_info.colorAttachmentCount    = 1;
	subpass_info.pColorAttachments       = &color_attachment_refs[0];
	subpass_info.pResolveAttachments     = nullptr;
	subpass_info.pDepthStencilAttachment = &depth_attachment_ref;
	subpass_info.preserveAttachmentCount = 0;
	subpass_info.pPreserveAttachments    = nullptr;

	VkSubpassDescription subpasses[] = { subpass_info };

	VkSubpassDependency dependency_info = {};
	dependency_info.srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependency_info.dstSubpass      = 0;
	dependency_info.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency_info.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency_info.srcAccessMask   = 0;
	dependency_info.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency_info.dependencyFlags = 0;

	VkRenderPassCreateInfo create_info{};
	create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.pNext           = nullptr;
	create_info.flags           = 0;
	create_info.attachmentCount = 2;
	create_info.pAttachments    = attachments;
	create_info.subpassCount    = 1;
	create_info.pSubpasses      = subpasses;
	create_info.dependencyCount = 1;
	create_info.pDependencies   = &dependency_info;

	VK_CHECK( vkCreateRenderPass(ctx.device, &create_info, nullptr, render_pass) );
	LOG_INFO("Renderpass created");
}

static void create_framebuffers(gpu_context_t& ctx)
{
	const int image_count = ctx.swapchain.image_views.size();
	ctx.framebuffers.resize(image_count);
	for (int i = 0; i < image_count; ++i)
	{
		VkImageView attachments[] = {
			ctx.swapchain.image_views[i],
			ctx.depth_buffer.view,
		};

		VkFramebufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.renderPass = ctx.render_pass;
		info.attachmentCount = 2;
		info.pAttachments = attachments;
		info.width  = ctx.swapchain.extent.width;
		info.height = ctx.swapchain.extent.height;
		info.layers = 1;

		VK_CHECK( vkCreateFramebuffer(ctx.device, &info, nullptr, &ctx.framebuffers[i]) );
	}
	LOG_INFO("Swapchain framebuffer created");
}

// Read a the given file path and returns a shader module
static void create_depth_buffer(gpu_context_t& ctx)
{
    create_image(ctx, VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT, 
            ctx.swapchain.extent.width, ctx.swapchain.extent.height,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
            VK_IMAGE_ASPECT_DEPTH_BIT, 
            &ctx.depth_buffer);
	LOG_INFO("Depth buffer created");
}

// TODO Special version of CreateImage -> just add sampler
bool create_texture(gpu_context_t& ctx, u32 width, u32 height, texture_t* texture)
{
    VkImageUsageFlagBits image_usage = (VkImageUsageFlagBits) (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    image_t image{};
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    create_image(ctx, VK_IMAGE_TYPE_2D, format, width, height, image_usage, VK_IMAGE_ASPECT_COLOR_BIT, &image);
    texture->image = image.handle;
    texture->view  = image.view;
    texture->allocation = image.allocation;
    create_sampler(ctx, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &texture->sampler);
    return true;
}

bool create_image(gpu_context_t& ctx,
        VkImageType image_type, 
        VkFormat format, 
        u32 width, u32 height, 
        VkImageUsageFlagBits usage, 
        VkImageAspectFlags aspect_mask, 
        image_t* p_image)
{
    VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;
	image_info.imageType = image_type;
	image_info.format = format;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = usage; //VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( vkCreateImage(ctx.device, &image_info, nullptr, &p_image->handle) );

	ctx.allocator.allocate_image_memory(p_image->handle, image_info.tiling, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, p_image->allocation);

    VkImageViewCreateInfo view{};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.pNext = nullptr;
	view.flags = 0;
	view.image = p_image->handle;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D; // TODO function parameter or infer from image_type
	view.format = format;
	view.subresourceRange = {
		aspect_mask,
		0, 1, 0, 1,
	};

	VK_CHECK( vkCreateImageView(ctx.device, &view, nullptr, &p_image->view) );
    return true;
}

bool create_sampler(gpu_context_t& ctx, VkFilter filter, VkSamplerAddressMode address_mode, VkSampler* p_sampler)
{
	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.magFilter = filter;
	info.minFilter = filter;
	info.addressModeU = address_mode;
	info.addressModeV = address_mode;
	info.addressModeW = address_mode;
	info.anisotropyEnable = VK_FALSE; // needs to be enabled on the device
	info.maxAnisotropy = 1.f;
	info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	info.compareEnable = VK_FALSE;
	info.compareOp = VK_COMPARE_OP_ALWAYS;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	info.mipLodBias = 0.f;
	info.minLod = 0.f;
	info.maxLod = 0.f;
	info.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( vkCreateSampler(ctx.device, &info, nullptr, p_sampler) );
    return true;
}

bool create_buffer(gpu_context_t& ctx, u32 size, u32 usage, buffer_t* buffer, VkMemoryPropertyFlags mem_flags) 
{
    if (mem_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
	VkBufferCreateInfo buffer_info{};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.usage = static_cast<VkBufferUsageFlagBits>(usage);
	buffer_info.size = size;
	buffer_info.queueFamilyIndexCount = 0;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.flags = 0;
	VK_CHECK( vkCreateBuffer(ctx.device, &buffer_info, nullptr, &buffer->handle) );

	ctx.allocator.allocate_buffer_memory(buffer->handle, mem_flags, buffer->allocation);
	return true;
}

void destroy_buffer(gpu_context_t& ctx, buffer_t& buffer)
{
    ctx.allocator.dealloc(buffer.allocation);
    vkDestroyBuffer(ctx.device, buffer.handle, nullptr);
}

VkResult begin_command_buffer(VkCommandBuffer cmd)
{
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = nullptr;
    begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    return vkBeginCommandBuffer(cmd, &begin);
}

VkCommandBuffer begin_one_time_command_buffer(gpu_context_t& ctx, VkCommandPool pool)
{
	VkCommandBufferAllocateInfo alloc{};
	alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc.pNext = nullptr;
	alloc.commandPool = pool;
	alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc.commandBufferCount = 1;

	VkCommandBuffer cmd;
	VK_CHECK( vkAllocateCommandBuffers(ctx.device, &alloc, &cmd) );

	VkCommandBufferBeginInfo begin{};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.pNext = nullptr;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin.pInheritanceInfo = nullptr;

	VK_CHECK( vkBeginCommandBuffer(cmd, &begin) );
    return cmd;
}

void end_and_submit_command_buffer(VkCommandBuffer cmd, VkQueue queue, VkSemaphore* wait_semaphore, VkPipelineStageFlags wait_stage)
{
	VK_CHECK( vkEndCommandBuffer(cmd) );

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
    if (wait_semaphore)
    {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
    }
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	VK_CHECK( vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) );
}

VkDeviceAddress get_buffer_device_address(gpu_context_t& ctx, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buffer;
    VkDeviceAddress address = vkGetBufferDeviceAddress(ctx.device, &info);
    if (!address)
    {
        LOG_ERROR("Unable to get buffer device address");
    }
    return address;
}

void write_buffer(gpu_context_t& ctx, buffer_t& buffer, u32 size, void* data)
{
	u8* p_data;
	ctx.allocator.map_memory(buffer.allocation, (void**)&p_data);
	memcpy(p_data, data, size);
	ctx.allocator.unmap_memory(buffer.allocation);
}

void destroy_image(gpu_context_t& ctx, image_t& image)
{
	vkDestroyImageView(ctx.device, image.view, nullptr);
	vkDestroyImage(ctx.device, image.handle, nullptr);
	ctx.allocator.dealloc(image.allocation);
}

void prepare_frame(gpu_context_t& ctx, i32* frame_index, frame_t **p_frame)
{
    static i32 _fidx = 0;
    *frame_index = _fidx;
    *p_frame = &ctx.frames[_fidx];
    auto frame = &ctx.frames[_fidx];
    VK_CHECK( vkWaitForFences(ctx.device, 1, &frame->fence, VK_TRUE, ONE_SECOND_IN_NANOSECONDS) );
    vkResetFences(ctx.device, 1, &frame->fence);
    _fidx = (_fidx + 1) % ctx.frames.size();
}

void get_next_swapchain_image(gpu_context_t& ctx, frame_t* frame)
{
    VkResult res = vkAcquireNextImageKHR(ctx.device, ctx.swapchain.handle, UINT64_MAX, 
            frame->present_semaphore, VK_NULL_HANDLE, &ctx.swapchain.image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    {
        // get window size and recreate swapchain and related resources
        return;
    }
    VK_CHECK(res);
}

void graphics_submit_frame(gpu_context_t& ctx, frame_t* frame)
{
    static VkPipelineStageFlags wait_dst_stage_mask = 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext = nullptr;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &frame->present_semaphore;
    info.pWaitDstStageMask = &wait_dst_stage_mask;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &frame->command_buffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &frame->render_semaphore;

    VK_CHECK( vkQueueSubmit(ctx.q_graphics, 1, &info, frame->fence) );
}

void present_frame(gpu_context_t& ctx, frame_t* frame)
{
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext = nullptr;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &frame->render_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &ctx.swapchain.handle;
    info.pImageIndices = &ctx.swapchain.image_index;
    info.pResults = nullptr;
    
    VkResult res = vkQueuePresentKHR(ctx.q_present, &info);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    {
        // get window size and recreate swapchain and related resources
    }
}

// default render pass renders to swapchain framebuffer
void begin_render_pass(gpu_context_t& ctx, VkCommandBuffer cmd, VkClearValue *clear, u32 clear_count)
{
    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.renderPass = ctx.render_pass;
    begin_info.framebuffer = ctx.framebuffers[ctx.swapchain.image_index];
    begin_info.renderArea = { {0,0}, ctx.swapchain.extent };
    begin_info.clearValueCount = clear_count;
    begin_info.pClearValues = clear;

    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

static void cleanup_swapchain(gpu_context_t& ctx)
{
	// destroy framebuffers
	for (size_t i = 0; i < ctx.framebuffers.size(); ++i)
	{
		vkDestroyFramebuffer(ctx.device, ctx.framebuffers[i], nullptr);
	}

	// reset all command buffers
	for (size_t i = 0; i < ctx.frames.size(); ++i)
	{
		vkResetCommandPool(ctx.device, ctx.frames[i].command_pool, 0);
	}

	// destroy graphics pipeline and default renderpass
	//vkDestroyPipeline(ctx.device, ctx.pipeline.handle, nullptr);
	//vkDestroyPipelineLayout(ctx.device, ctx.pipeline.layout , nullptr);
	vkDestroyRenderPass(ctx.device, ctx.render_pass, nullptr);

	// destroy swapchain image views
	for (const auto& view : ctx.swapchain.image_views)
	{
		vkDestroyImageView(ctx.device, view, nullptr);
	}

	// destroy depth buffer
	destroy_image(ctx, ctx.depth_buffer);

	vkDestroySwapchainKHR(ctx.device, ctx.swapchain.handle, nullptr);
	ctx.swapchain.handle = VK_NULL_HANDLE;
	LOG_INFO("Swapchain cleaned up");
}

void on_window_resize(gpu_context_t& ctx, window_t& window)
{	
	vkDeviceWaitIdle(ctx.device);
	LOG_INFO("Recreating swapchain");

	cleanup_swapchain(ctx);
	create_swapchain(ctx, &window);
	//create_render_pass(ctx);
	create_framebuffers(ctx);
	//create_graphics_pipeline(ctx, ctx.default_pipeline_description, ctx.pipeline);
}

// renderer needs to destroy it allocated resources (images, buffers etc)
void destroy_context(gpu_context_t& ctx)
{
	vkDeviceWaitIdle(ctx.device);
	cleanup_swapchain(ctx);
	
	//destroy frames
	for (size_t i = 0; i < ctx.frames.size(); ++i)
	{
		vkDestroySemaphore(ctx.device, ctx.frames[i].render_semaphore, nullptr);
		vkDestroySemaphore(ctx.device, ctx.frames[i].present_semaphore, nullptr);
		vkDestroyFence(ctx.device, ctx.frames[i].fence, nullptr);
		vkDestroyCommandPool(ctx.device, ctx.frames[i].command_pool, nullptr);
	}

	ctx.allocator.destroy_allocator();// free memory
	vkDestroyDevice(ctx.device, nullptr);
	vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
#ifdef _DEBUG
	destroy_debug_utils_messenger_ext(ctx.instance, ctx.debug_messenger, nullptr);
#endif
	vkDestroyInstance(ctx.instance, nullptr);
	LOG_INFO("Vulkan context destroyed");
}

