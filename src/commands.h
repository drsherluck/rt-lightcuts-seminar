#ifndef COMMANDS_H
#define COMMANDS_H

#include "common.h"

#include <vulkan/vulkan.h>
#include <vector>

struct command_allocator_t
{
    VkDevice device;
    VkCommandPool command_pool;

    size_t index;
    std::vector<VkCommandBuffer> command_buffers;
};

void init_command_allocator(VkDevice device, u32 queue_family_index, command_allocator_t& allocator);
void destroy_command_allocator(command_allocator_t& allocator);
VkCommandBuffer allocate_command_buffer(command_allocator_t& allocator);
void reset(command_allocator_t& allocator);

#endif // COMMANDS_H
