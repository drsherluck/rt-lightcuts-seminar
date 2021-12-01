#if 1
#include "commands.h"
#include "backend.h"

void init_command_allocator(VkDevice device, u32 queue_family_index, command_allocator_t& allocator)
{
    VkCommandPoolCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create.queueFamilyIndex = queue_family_index;
    VK_CHECK( vkCreateCommandPool(device, &create, nullptr, &allocator.command_pool) );
    allocator.device = device;
    allocator.index = 0;
}

VkCommandBuffer allocate_command_buffer(command_allocator_t& allocator)
{
    VkCommandBuffer cmd;
    auto& index = allocator.index;
    if (index == allocator.command_buffers.size())
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = allocator.command_pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;

        VK_CHECK( vkAllocateCommandBuffers(allocator.device, &info, &cmd) );
        allocator.command_buffers.push_back(cmd);
        LOG_INFO("ALLOCATED NEW COMMAND BUFFER");
    }
    cmd = allocator.command_buffers[index];
    index++;
    return cmd;
}

void reset(command_allocator_t& allocator)
{
    VK_CHECK( vkResetCommandPool(allocator.device, allocator.command_pool, 0) );
    allocator.index = 0;
}

void destroy_command_allocator(command_allocator_t& allocator)
{
    vkDestroyCommandPool(allocator.device, allocator.command_pool, nullptr);
}
#endif
