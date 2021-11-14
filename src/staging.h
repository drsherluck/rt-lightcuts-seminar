#ifndef STAGING_H
#define STAGING_H

#include "backend.h"

#define STAGING_DEFAULT_ALLOCATION_SIZE 64 * 1024 * 1024

/* Essentially a linear allocator
 * that gets reset after uploading data everytime */
struct staging_buffer_t
{
    gpu_context_t*  context;
    buffer_t        buffer;
    void*           mapped;
    VkDeviceSize    used_space;
    VkFence         fence = VK_NULL_HANDLE;
    VkSemaphore     upload_complete = VK_NULL_HANDLE;
    VkCommandPool   command_pool;
    VkCommandBuffer cmd;

    staging_buffer_t() = default;
    staging_buffer_t(gpu_context_t* context);
    ~staging_buffer_t();
};

void begin_upload(staging_buffer_t& staging);
// upload_complete used for sync between queus
void end_upload(staging_buffer_t& staging, VkSemaphore* upload_complete = nullptr);

void init_staging_buffer(staging_buffer_t& staging, gpu_context_t* p_context);
//void destroy_staging_buffer(staging_buffer_t& staging);
void copy_to_buffer(staging_buffer_t& staging, buffer_t& buffer, u32 size, void* data, u64 offset = 0);
void copy_to_image(staging_buffer_t& staging, VkImage image, u32 width, u32 height, void* data); 
//void reset(staging_buffer_t& staging);

#endif // STAGING_H
