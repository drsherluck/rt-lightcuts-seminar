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
inline void destroy_set_layout(VkDevice device, descriptor_set_layout_t& layout) { vkDestroyDescriptorSetLayout(device, layout.handle, nullptr); }

struct descriptor_allocator_t
{
    VkDevice device;
    VkDescriptorPool descriptor_pool;

    const std::vector<VkDescriptorPoolSize> pool_sizes = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 50 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 50 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 50 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 10 },
    };
};

void init_descriptor_allocator(VkDevice device, u32 max_set_count, descriptor_allocator_t* allocator);
bool allocate_descriptor_set(descriptor_allocator_t& d_allocator, VkDescriptorSetLayout layout, VkDescriptorSet* set);
inline void destroy_descriptor_allocator(descriptor_allocator_t& allocator) { vkDestroyDescriptorPool(allocator.device, allocator.descriptor_pool, nullptr); }

struct descriptor_set_t
{
    descriptor_set_layout_t layout;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_writes;

    descriptor_set_t(descriptor_set_layout_t set_layout) : layout(set_layout) {}
};

void bind_buffer(descriptor_set_t& set, u32 binding, VkDescriptorBufferInfo* info);
void bind_image(descriptor_set_t& set, u32 binding, VkDescriptorImageInfo* info);
void bind_acceleration_structure(descriptor_set_t& set, u32 binding, VkAccelerationStructureKHR* acceleration_struture);
VkDescriptorSet build_descriptor_set(descriptor_allocator_t& allocator, descriptor_set_t& set);

#endif
