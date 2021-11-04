#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include "backend.h"
#include <vector>

struct descriptor_set_layout_t
{
    VkDescriptorSetLayout                     handle = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

void add_binding(descriptor_set_layout_t& set_layout, u32 binding, VkDescriptorType type, VkShaderStageFlags stages);
VkDescriptorSetLayout build_descriptor_set_layout(VkDevice device, descriptor_set_layout_t& set_layout);

struct descriptor_allocator_t
{
    VkDevice device;
    VkDescriptorPool descriptor_pool;

    const std::vector<VkDescriptorPoolSize> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };
};

void init_descriptor_allocator(VkDevice device, u32 max_set_count, descriptor_allocator_t* allocator);
bool allocate_descriptor_set(descriptor_allocator_t& d_allocator, VkDescriptorSetLayout layout, VkDescriptorSet* set);

struct descriptor_set_t
{
    descriptor_set_layout_t layout;
    std::vector<VkWriteDescriptorSet> writes;

    descriptor_set_t(descriptor_set_layout_t set_layout) : layout(set_layout) {}
};

void bind_buffer(descriptor_set_t& set, u32 binding, VkDescriptorBufferInfo* info);
void bind_image(descriptor_set_t& set, u32 binding, VkDescriptorImageInfo* info);
VkDescriptorSet build_descriptor_set(descriptor_allocator_t& allocator, descriptor_set_t& set);

#endif
