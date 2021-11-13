#include "allocator.h"
#include "log.h"

#include <cstdio>
#include <assert.h>

// log allocations and frees
//#define VK_ALLOCATOR_LOG_EVENTS


//https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#memory-device
static bool find_properties(const VkPhysicalDeviceMemoryProperties &mem_properties,
		u32 mem_type_bits_requirements,
		VkMemoryPropertyFlags required_properties,
		u32* index)
{
	for (u32 i = 0; i < mem_properties.memoryTypeCount; ++i)
	{
		const u32 memoryTypeBits = (1 << i);
		const bool is_required_type = mem_type_bits_requirements & memoryTypeBits;
		const VkMemoryPropertyFlags properties = mem_properties.memoryTypes[i].propertyFlags;
		const bool has_required_props = (properties & required_properties) == required_properties;

		if (is_required_type && has_required_props)
		{
			*index = i;
			return true;
		}
	}
	return false;
}

void vk_allocator_t::init_allocator(VkDevice device, VkPhysicalDevice physical_device)
{
	this->device = device;
	this->physical_device = physical_device;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
	vkGetPhysicalDeviceProperties(physical_device, &device_properties);
	heaps.resize(memory_properties.memoryHeapCount);
	LOG_INFO("Vulkan memory allocator initialized");
}

void vk_allocator_t::destroy_allocator()
{
	for (size_t i = 0; i < heaps.size(); ++i)
	{
		// free linear allocations
		for (size_t j = 0; j < heaps[i].linear.size(); ++j)
		{
			vkFreeMemory(device, heaps[i].linear[j].memory_handle, nullptr);
		}
		// free non_linear allocations
		for (size_t j = 0; j < heaps[i].non_linear.size(); ++j)
		{
			vkFreeMemory(device, heaps[i].non_linear[j].memory_handle, nullptr);
		}
	}
	LOG_INFO("Vulkan memory freed");
}

void vk_allocator_t::allocate_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags flags, allocation_t& allocation)
{
	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
	allocate(ALLOC_TYPE_BUFFER, memory_requirements, flags, allocation);
	if (vkBindBufferMemory(device, buffer, allocation.memory_handle, allocation.offset) != VK_SUCCESS)
	{
		LOG_ERROR("Could not bind buffer to memory section");
	}
}

void vk_allocator_t::allocate_image_memory(VkImage image, 
		VkImageTiling tiling, 
		VkMemoryPropertyFlags flags, 
		allocation_t& allocation)
{
	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(device, image, &memory_requirements);
	allocation_type_t type = tiling == VK_IMAGE_TILING_OPTIMAL 
		? ALLOC_TYPE_IMAGE_OPTIMAL : ALLOC_TYPE_IMAGE_LINEAR;
	allocate(type, memory_requirements, flags, allocation);
	if (vkBindImageMemory(device, image, allocation.memory_handle, allocation.offset) != VK_SUCCESS)
	{
		LOG_ERROR("Could not bind image to memory section");
	}
}

void vk_allocator_t::allocate_new_block(u32 heap_index, u32 type_index, u32 required_size, block_t& block)
{
	block.id = block_count++; // set afterwards
	block.heap_index = heap_index;
	block.size = DEFAULT_ALLOCATION_SIZE;
	block.p_head = nullptr;
    
    while (block.size < required_size)
    {
        block.size += DEFAULT_ALLOCATION_SIZE;
    }

    // Maybe not effecient to mark every block with this flag
    VkMemoryAllocateFlagsInfo flags{};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.pNext = nullptr;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    flags.deviceMask = 0;

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = &flags;
	alloc_info.allocationSize = block.size;
	alloc_info.memoryTypeIndex = type_index;

	if (vkAllocateMemory(device, &alloc_info, nullptr, &block.memory_handle) != VK_SUCCESS)
	{
		LOG_ERROR("Unable to allocate block of memory");
	}
#ifdef VK_ALLOCATOR_LOG_EVENTS
	LOG_INFO("Allocated block %ld of size %ld bytes", block.id, block.size);
#endif

    memory_stats.allocated_size += block.size;
    memory_stats.free_size += block.size;
}

inline static sub_allocation_t* create_free_sub_allocation(u64 id, u64 block_id, VkDeviceMemory memory_handle, u64 start, u64 size)
{
    sub_allocation_t* sub = (sub_allocation_t*)malloc(sizeof(sub_allocation_t));
    sub->type = ALLOC_TYPE_FREE;
    sub->id = id;
    sub->block_id = block_id;
    sub->memory_handle = memory_handle;
    sub->start = start;
    sub->size = size;
    sub->p_next = nullptr;
    return sub;
}

void vk_allocator_t::allocate(allocation_type_t alloc_type, 
		VkMemoryRequirements requirements, 
		VkMemoryPropertyFlags flags, 
		allocation_t& allocation)
{
	u32 type_index;
	if (!find_properties(memory_properties, requirements.memoryTypeBits, flags, &type_index))
	{
		LOG_ERROR("Could not find memory type");
	}
	u32 heap_index = memory_properties.memoryTypes[type_index].heapIndex;

	std::vector<block_t>* pool = &heaps[heap_index].linear;
	if (alloc_type == ALLOC_TYPE_IMAGE_OPTIMAL)
	{
		pool = &heaps[heap_index].non_linear;
	}

	// find sub allocation space
	sub_allocation_t* alloc = nullptr;
	VkDeviceSize max_size = 0;
	for (size_t i = 0; i < pool->size(); ++i)
	{
		sub_allocation_t* temp = pool->at(i).p_head;
		while (temp != nullptr)
		{
			if (temp->type == ALLOC_TYPE_FREE && temp->size > requirements.size && temp->size > max_size)
			{
				max_size = temp->size;	
				alloc = temp;
			}
			temp = temp->p_next;
		}
	}

	if (!alloc)
	{
		pool->emplace_back();
		block_t& new_block = pool->back();
		allocate_new_block(heap_index, type_index, requirements.size, new_block);
		new_block.id = pool->size();

		// create free suballocation
		allocation_count++;
        alloc = create_free_sub_allocation(allocation_count, new_block.id, new_block.memory_handle, 0, new_block.size);
		// point to this sub allocation
		new_block.p_head = alloc;
	}
	// perform allocation
	VkDeviceSize offset = (alloc->start + requirements.alignment - 1) & ~(requirements.alignment - 1);
	if ((alloc->start + alloc->size) < (offset + requirements.size))
	{
		LOG_ERROR("allocation_t exceeds available sub allocation space");
		// TODO Create new block 
	}

	// create new sub allocation from left over space
	// and link old sub allocation with next sub allocation
	VkDeviceSize next_size = (alloc->start + alloc->size) - (offset + requirements.size);
	if (next_size > 0)
	{
        allocation_count++;
        auto new_sub = create_free_sub_allocation(allocation_count, alloc->block_id, alloc->memory_handle,
                offset + requirements.size, next_size);
		// alloc -> new_sub -> alloc->p_next
		new_sub->p_next = alloc->p_next;
		alloc->p_next = new_sub;
	}

	// update sub allocation
	alloc->type = alloc_type;

	// set allocation attributes
	allocation.id = alloc->id;
	allocation.block_id = alloc->block_id;
	allocation.heap_index = heap_index;
	allocation.memory_handle = alloc->memory_handle;
	allocation.type = alloc_type;
	allocation.size = requirements.size;
	allocation.offset = offset;

#ifdef VK_ALLOCATOR_LOG_EVENTS
	LOG_INFO("Allocation %ld | size = %ld, offset = %ld, heap = %d, block_id = %ld",
			allocation.id, allocation.size, allocation.offset, heap_index, allocation.block_id);
#endif
    memory_stats.used_size += allocation.size;
    memory_stats.free_size -= allocation.size;
}

void vk_allocator_t::dealloc(allocation_t& allocation)
{
	std::vector<block_t>& pool = allocation.type == ALLOC_TYPE_IMAGE_OPTIMAL ?
		heaps[allocation.heap_index].non_linear : heaps[allocation.heap_index].linear;

	for (size_t i = 0; i < pool.size(); ++i)
	{
		sub_allocation_t* p_prev = nullptr;
		sub_allocation_t* p_curr = pool[i].p_head;	
		while (p_curr != nullptr)
		{
			if (p_curr->id == allocation.id)
			{
				allocation.type = ALLOC_TYPE_FREE;
				p_curr->type = ALLOC_TYPE_FREE;
                while (p_curr->p_next && p_curr->p_next->type == ALLOC_TYPE_FREE)
                {
                    auto p_next = p_curr->p_next;
                    p_curr->p_next = p_next->p_next;
                    free(p_next);
                }
                if (p_prev && p_prev->type == ALLOC_TYPE_FREE)
                {
                    p_prev->p_next = p_curr->p_next;
                    free(p_curr);
                }
                memory_stats.free_size += allocation.size;
                memory_stats.used_size -= allocation.size;
				return;
			}
			p_prev = p_curr;
			p_curr = p_curr->p_next;
		}
	}
	LOG_ERROR("Unkown allocation %ld, could not free given allocation", allocation.id);
}

void vk_allocator_t::map_memory(allocation_t& alloc, void** pp_data)
{
	std::vector<block_t>& pool = alloc.type == ALLOC_TYPE_IMAGE_OPTIMAL ?
		heaps[alloc.heap_index].non_linear : heaps[alloc.heap_index].linear;

    for (size_t i = 0; i < pool.size(); ++i)
    {
        block_t& block = pool[i];
        if (alloc.block_id == block.id)
        {
            if (!block.p_mapped)
            {
                VkResult res = vkMapMemory(device, block.memory_handle, 0, VK_WHOLE_SIZE, 0, &block.p_mapped);
                if (res != VK_SUCCESS)
                {
                    LOG_ERROR("Failed to map memory");
                }
                LOG_INFO("First mapping of memory block");
            }
            *pp_data = (u8*)block.p_mapped + alloc.offset;
        }
    }
}

void vk_allocator_t::unmap_memory(allocation_t& alloc)
{
	VkMappedMemoryRange range{};
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = nullptr;
	range.memory = alloc.memory_handle;
	range.offset = alloc.offset;
	range.size = alloc.size;

	vkFlushMappedMemoryRanges(device, 1, &range);
	//vkUnmapMemory(device, alloc.memory_handle);
}

static void human_readable_bytes(u64 bytes, char buffer[10])
{
    const f64 kb = f64(1024);
    f64 b = static_cast<f64>(bytes)/kb;
    u32 l = 1;
    while (b >= 1)
    {
        b /= kb;
        l++;
    }
    b *= kb;
    l--;
    switch (l)
    {
        case 0: sprintf(buffer, "%.2f b", b); break;
        case 1: sprintf(buffer, "%.2f Kb", b); break;
        case 2: sprintf(buffer, "%.2f Mb", b); break;
        case 3: sprintf(buffer, "%.2f Gb", b); break;
        default: break;
    }
}

void vk_allocator_t::print_memory_usage()
{
    char allocated_size[10], free_size[10], used_size[10];
    human_readable_bytes(memory_stats.allocated_size, allocated_size);
    human_readable_bytes(memory_stats.free_size, free_size);
    human_readable_bytes(memory_stats.used_size, used_size);
    LOG_INFO("\ngpu_memory\n{\n\tallocated = %s\n\tused = %s\n\tfree = %s\n}", 
            allocated_size, used_size, free_size);
}

