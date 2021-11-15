#ifndef _VK_ALLOCATOR_H
#define _VK_ALLOCATOR_H

#include "common.h"
#include "math.h"

#include <vulkan/vulkan.h>
#include <vector>
#define USE_VMA
#ifdef USE_VMA
#include <vk_mem_alloc.h>
#endif

#define DEFAULT_ALLOCATION_SIZE 32 * 1024 * 1024 // 128 Mb
#define DEFAULT_BLOCK_SIZE 524288 // 514 Kb

enum allocation_type_t
{
	ALLOC_TYPE_FREE = 0,
	ALLOC_TYPE_BUFFER = 1,
	ALLOC_TYPE_IMAGE_LINEAR = 2,
	ALLOC_TYPE_IMAGE_OPTIMAL = 4
};

struct allocation_t
{
	u64               id;
	u64               block_id;
	u32	              heap_index;
	allocation_type_t type;
	VkDeviceMemory    memory_handle;
	VkDeviceSize      size;
	VkDeviceSize      offset;
#ifdef USE_VMA
    VmaAllocation     _vma_alloc;
#endif 
};

struct sub_allocation_t
{
	u64               id;
	u64               block_id;
	allocation_type_t type;
	VkDeviceMemory    memory_handle; // might not be needed
	VkDeviceSize      start;
	VkDeviceSize      size;
	sub_allocation_t* p_next;
};

struct block_t
{
	u64               id;
	u32               heap_index;
    void*             p_mapped = nullptr;
	VkDeviceMemory    memory_handle;
	VkDeviceSize      size;
	sub_allocation_t* p_head;
};

struct heap_t
{
	std::vector<block_t> linear;
	std::vector<block_t> non_linear;
};

struct memory_stats_t
{
    u64 allocated_size = 0;
    u64 used_size = 0;
    u64 free_size = 0;
};

struct vk_allocator_t
{
	VkDevice                         device;
	VkPhysicalDevice                 physical_device;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkPhysicalDeviceProperties       device_properties;
	u64                              allocation_count = 0;
	u64                              block_count = 0;
    memory_stats_t                   memory_stats;

	std::vector<heap_t> heaps;

#ifdef USE_VMA
    VmaAllocator vma;
#endif 

	void init_allocator(VkDevice device, VkPhysicalDevice physical_device, VkInstance instance);
	void destroy_allocator();
    
    void allocate_new_block(u32 heap_index, u32 type_index, u32 required_size, block_t& block);
	void allocate_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags flags, allocation_t& allocation);
	void allocate_image_memory(VkImage image, 
			VkImageTiling tiling, 
			VkMemoryPropertyFlags flags, 
			allocation_t& allocation);
	void allocate(allocation_type_t alloc_type, 
			VkMemoryRequirements requirements, 
			VkMemoryPropertyFlags flags, 
			allocation_t& alloc);
	void dealloc(allocation_t& alloc);

	void map_memory(allocation_t& alloc, void** pp_data);
	void unmap_memory(allocation_t& alloc);

    void print_memory_usage();
};


#endif  // _VK_ALLOCATOR_H
