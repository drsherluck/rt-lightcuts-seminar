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

void init_staging_buffer(staging_buffer_t& staging, gpu_context_t* p_context)
{
    assert(p_context);
    staging.context = p_context;
    create_buffer(*staging.context, STAGING_DEFAULT_ALLOCATION, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging.buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    p_context->allocator.map_memory(staging.buffer.allocation, (void**)&staging.mapped);
}

void destroy_staging_buffer(staging_buffer_t& staging)
{
    staging.context->allocator.unmap_memory(staging.buffer.allocation);
    destroy_buffer(*staging.context, staging.buffer);
}

void copy_to_buffer(staging_buffer_t& staging, VkCommandBuffer cmd, buffer_t& dst, u32 size, void* data, u64 offset)
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
   
    vkCmdCopyBuffer(cmd, staging.buffer.handle, dst.handle, 1, &copy);
}

void copy_to_image(staging_buffer_t& staging, VkCommandBuffer cmd, VkImage image, u32 width, u32 height, void* data)
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

	vkCmdCopyBufferToImage(cmd, staging.buffer.handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,  &copy);
}

void reset(staging_buffer_t& staging)
{
    staging.used_space = 0;
}
