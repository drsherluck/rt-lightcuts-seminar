#include "staging.h"
#include "log.h"

#include <cassert>
#include <cstring>

static void* get_next_mapped_data(staging_buffer_t& staging, VkDeviceSize& offset, u32 size)
{
    assert(staging.used_space + size < staging.buffer.allocation.size);
    offset = staging.used_space;
    staging.used_space += size;
    return static_cast<void*>((u8*)staging.mapped + offset);
}

void begin_upload(staging_buffer_t& staging)
{
    VK_CHECK( vkWaitForFences(staging.context->device, 1, &staging.fence, VK_TRUE, ONE_SECOND_IN_NANOSECONDS) );
    vkResetFences(staging.context->device, 1, &staging.fence);
    VK_CHECK( vkResetCommandPool(staging.context->device, staging.command_pool, 0) );
    staging.used_space = 0;
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;
    VK_CHECK( vkBeginCommandBuffer(staging.cmd, &begin_info) );
}

void end_upload(staging_buffer_t& staging, VkSemaphore* upload_complete)
{
    if (upload_complete) 
    {
        *upload_complete = staging.upload_complete;
    }
    
    VK_CHECK( vkEndCommandBuffer(staging.cmd) );
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 0;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &staging.cmd;
    if (upload_complete)
    {
        submit_info.signalSemaphoreCount = 1; 
        submit_info.pSignalSemaphores = &staging.upload_complete;
    }
    VK_CHECK( vkQueueSubmit(staging.context->q_transfer, 1, &submit_info, staging.fence) );
}

void init_staging_buffer(staging_buffer_t& staging, gpu_context_t* _context)
{
    assert(_context);
    staging.context = _context;
    staging.used_space = 0;
    create_buffer(*(_context), STAGING_DEFAULT_ALLOCATION_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging.buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _context->allocator.map_memory(staging.buffer.allocation, (void**)&staging.mapped);
    
    VkFenceCreateInfo fence_create = {};
    fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK( vkCreateFence(_context->device, &fence_create, nullptr, &staging.fence) );

    VkCommandPoolCreateInfo pool_create = {};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = _context->q_transfer_index;
    VK_CHECK( vkCreateCommandPool(_context->device, &pool_create, nullptr, &staging.command_pool) );

    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = staging.command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VK_CHECK( vkAllocateCommandBuffers(_context->device, &info, &staging.cmd) );

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK( vkCreateSemaphore(staging.context->device, &semaphore_info, nullptr, &staging.upload_complete) );
}


void destroy_staging_buffer(staging_buffer_t& staging)
{
    VK_CHECK( vkWaitForFences(staging.context->device, 1, &staging.fence, VK_TRUE, ONE_SECOND_IN_NANOSECONDS) );
    VK_CHECK( vkResetCommandPool(staging.context->device, staging.command_pool, 0) );
    staging.context->allocator.unmap_memory(staging.buffer.allocation);
    destroy_buffer(*(staging.context), staging.buffer);
    vkDestroyFence(staging.context->device, staging.fence, nullptr);
    vkDestroySemaphore(staging.context->device, staging.upload_complete, nullptr);
    vkDestroyCommandPool(staging.context->device, staging.command_pool, nullptr);
}

void copy_to_buffer(staging_buffer_t& staging, buffer_t& dst, u32 size, void* data, u64 offset)
{
    assert(dst.handle);

    VkDeviceSize src_offset;
    void* p_data = get_next_mapped_data(staging, src_offset, size);

    if (data)
    {
        memcpy(p_data, data, size);
    }

    VkBufferCopy copy{};
    copy.srcOffset = src_offset;
    copy.dstOffset = offset;
    copy.size = size;
   
    vkCmdCopyBuffer(staging.cmd, staging.buffer.handle, dst.handle, 1, &copy);
}

void copy_to_image(staging_buffer_t& staging, VkImage image, u32 width, u32 height, void* data)
{
    u64 size = static_cast<u64>(width * height * sizeof(f32));

    VkDeviceSize offset;
    void* p_data = get_next_mapped_data(staging, offset, size);

    if (data)
    {
        memcpy(p_data, data, size);
    }

	VkBufferImageCopy copy{};
	copy.bufferOffset = offset;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	copy.imageOffset = {0, 0, 0};
	copy.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(staging.cmd, staging.buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,  &copy);
}

/*void reset(staging_buffer_t& staging)
{
    staging.used_space = 0;
}*/
