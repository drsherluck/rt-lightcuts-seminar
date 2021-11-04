#include "descriptor.h"
#include "log.h"

#include <cassert>

void add_binding(descriptor_set_layout_t& set_layout, u32 binding, VkDescriptorType type, VkShaderStageFlags stages)
{
    VkDescriptorSetLayoutBinding layout = {};
    layout.binding = binding;
    layout.descriptorType = type;
    layout.descriptorCount = 1;
    layout.stageFlags = stages;
    layout.pImmutableSamplers = nullptr;
    set_layout.bindings.push_back(layout);
}

VkDescriptorSetLayout build_descriptor_set_layout(VkDevice device, descriptor_set_layout_t& set_layout)
{
    if (set_layout.handle == VK_NULL_HANDLE) 
    {
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<u32>(set_layout.bindings.size());
        info.pBindings = set_layout.bindings.data();

        VK_CHECK( vkCreateDescriptorSetLayout(device, &info, nullptr, &set_layout.handle) );
    }
    return set_layout.handle;
}

void init_descriptor_allocator(VkDevice device, u32 max_set_count, descriptor_allocator_t* allocator)
{
    assert(allocator);
    allocator->device = device;

    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = max_set_count;
    info.poolSizeCount = static_cast<u32>(allocator->pool_sizes.size());
    info.pPoolSizes = allocator->pool_sizes.data();

    VK_CHECK( vkCreateDescriptorPool(device, &info, nullptr, &allocator->descriptor_pool) );
}

bool allocate_descriptor_set(descriptor_allocator_t& allocator, VkDescriptorSetLayout layout, VkDescriptorSet* set)
{
    assert(set);
    VkDescriptorSetAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = allocator.descriptor_pool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &layout;
    VK_CHECK( vkAllocateDescriptorSets(allocator.device, &allocation, set) );
    return true;
}

void bind_buffer(descriptor_set_t& set, u32 binding, VkDescriptorBufferInfo* info)
{
    auto& bind = set.layout.bindings[binding];

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = bind.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = bind.descriptorCount;
    write.descriptorType = bind.descriptorType;
    write.pBufferInfo = info;
    set.writes.push_back(write);
}

void bind_image(descriptor_set_t& set, u32 binding, VkDescriptorImageInfo* info)
{
    auto& bind = set.layout.bindings[binding];

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = bind.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = bind.descriptorCount;
    write.descriptorType = bind.descriptorType;
    write.pImageInfo = info;
    set.writes.push_back(write);
}
    
VkDescriptorSet build_descriptor_set(descriptor_allocator_t& allocator, descriptor_set_t& set) 
{
    VkDescriptorSet _set;
    allocate_descriptor_set(allocator, set.layout.handle, &_set);

    for (auto& write : set.writes)
    {
        write.dstSet = _set;
    }
    
    vkUpdateDescriptorSets(allocator.device, static_cast<u32>(set.writes.size()), set.writes.data(), 0, nullptr);
    return _set;
}

