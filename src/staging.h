#ifndef STAGING_H
#define STAGING_H

#include "backend.h"

#define STAGING_DEFAULT_ALLOCATION 16 * 1024 * 1024

/* Essentially a linear allocator
 * that gets reset after uploading data everytime */
struct staging_buffer_t
{
    gpu_context_t* context;
    buffer_t       buffer;
    void*          mapped;
    VkDeviceSize   used_space = 0;
};

void init_staging_buffer(staging_buffer_t& staging, gpu_context_t* p_context);
void destroy_staging_buffer(staging_buffer_t& staging);
void copy_to_buffer(staging_buffer_t& staging, VkCommandBuffer cmd, buffer_t& buffer, u32 size, void* data, u64 offset = 0);
void copy_to_image(staging_buffer_t& staging, VkCommandBuffer cmd, VkImage image, u32 width, u32 height, void* data); 
void reset(staging_buffer_t& staging);

#endif // STAGING_H
