#include "acceleration_struct.h"
#include "backend.h"
#include "log.h"
#include "debug_util.h"

#include <cassert>
#include <utility>

void init_acceleration_structure_builder(acceleration_structure_builder_t& builder, gpu_context_t* ctx)
{
    LOG_INFO("INIT AS BUILDER");
    builder.context = ctx;
    init_staging_buffer(builder.staging, ctx);
    init_command_allocator(ctx->device, ctx->q_compute_index, builder.command_allocator);
}

static VkAccelerationStructureKHR create_acceleration_structure(
        VkDevice device,
        VkAccelerationStructureTypeKHR type,
        VkBuffer buffer, 
        VkDeviceSize size, VkDeviceSize offset = 0)
{
    assert((type & VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR) == 0);

    VkAccelerationStructureCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    info.pNext = nullptr;
    info.createFlags = 0;
    info.buffer = buffer;
    info.offset = offset;
    info.size = size;
    info.type = type; 
    info.deviceAddress = 0; // not using accelerationStructureCaptureReplay for now

    VkAccelerationStructureKHR as;
    VK_CHECK( vkCreateAccelerationStructure(device, &info, nullptr, &as) );
    return as;
}


void build_tlas(acceleration_structure_builder_t& builder,
        std::vector<VkAccelerationStructureInstanceKHR>& instances,
        acceleration_structure_t& as, bool update) 
{
    VkBuildAccelerationStructureFlagsKHR build_flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    gpu_context_t& ctx = *(builder.context);

    // create buffer for instance(s)
    u32 size_instance_buffer = static_cast<u32>(instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
    buffer_t instance_buffer;
    create_buffer(ctx, size_instance_buffer, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, &instance_buffer);

    VkSemaphore upload_complete;
    begin_upload(builder.staging);
    copy_to_buffer(builder.staging, instance_buffer, size_instance_buffer, (void*)instances.data());
    end_upload(builder.staging, &upload_complete);

    // wait memory write
    /*VkMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0, 1, &barrier, 0, nullptr, 0, nullptr);*/


    VkAccelerationStructureGeometryInstancesDataKHR instance_data{};
    instance_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instance_data.arrayOfPointers = VK_FALSE;
    instance_data.data.deviceAddress = get_buffer_device_address(ctx, instance_buffer.handle);

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    geom.geometry.instances = instance_data;

    // base info for size query
    VkAccelerationStructureBuildGeometryInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    info.pNext = nullptr;
    info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    info.flags = build_flags;
    info.mode  = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.srcAccelerationStructure = VK_NULL_HANDLE; // only for update
    info.dstAccelerationStructure = VK_NULL_HANDLE;
    info.geometryCount = 1; 
    info.pGeometries = &geom;

    u32 count_instance = static_cast<u32>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR required_size;
    required_size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizes(ctx.device, 
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &info, &count_instance, &required_size);

    // create scratch buffer
    buffer_t scratch_buffer;
    create_buffer(ctx, required_size.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &scratch_buffer);
    
    // create acceleration structure buffer
    VkAccelerationStructureKHR acceleration_struct = as.handle;
    if (!update)
    {
        buffer_t as_buffer; // TODO Leak/Save somewhere 
        create_buffer(ctx, required_size.accelerationStructureSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, &as_buffer);

        acceleration_struct = create_acceleration_structure(ctx.device, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 
                as_buffer.handle, required_size.accelerationStructureSize);
        as.handle = acceleration_struct;
        as.buffer = as_buffer;
    }

    VkCommandBuffer cmd = allocate_command_buffer(builder.command_allocator);
    VK_CHECK( begin_command_buffer(cmd) );

    VkAccelerationStructureBuildGeometryInfoKHR build_info = info; // copy info
    build_info.srcAccelerationStructure = update ? acceleration_struct : VK_NULL_HANDLE;
    build_info.dstAccelerationStructure = acceleration_struct;
    build_info.scratchData.deviceAddress = get_buffer_device_address(ctx, scratch_buffer.handle);

    VkAccelerationStructureBuildRangeInfoKHR build_range = { count_instance, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_range = &build_range;
    vkCmdBuildAccelerationStructures(cmd, 1, &build_info, &p_build_range);
    end_and_submit_command_buffer(cmd, ctx.q_compute, &upload_complete, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

    VkResult res = wait_for_queue(ctx.q_compute);
    if (res != VK_SUCCESS)
    {
        PRINT_CHECKPOINT_STACK(ctx.q_compute);
        VK_CHECK(res);
    }
    
    reset(builder.command_allocator);
    destroy_buffer(ctx, instance_buffer);
    destroy_buffer(ctx, scratch_buffer);
}

void build_blases(acceleration_structure_builder_t& builder, 
        std::vector<acceleration_structure_t>& out)
{
    gpu_context_t& ctx = *(builder.context);
    u32 blas_count = static_cast<u32>(builder.inputs.size());
    VkDeviceSize max_scratch_buffer_size = 0;
    VkDeviceSize buffer_size = 0;

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blas_infos(blas_count);
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> size_infos(blas_count);
    for (u32 i = 0; i < blas_count; ++i)
    {
        auto& input = builder.inputs[i];
        auto& blas_info = blas_infos[i];
        blas_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        blas_info.pNext = nullptr;
        blas_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blas_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        blas_info.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blas_info.srcAccelerationStructure = VK_NULL_HANDLE; // only update
        blas_info.dstAccelerationStructure = VK_NULL_HANDLE;
        blas_info.geometryCount = 1;
        blas_info.pGeometries = &input.geometry;
        blas_info.ppGeometries = nullptr;

        auto& size_info = size_infos[i];
        size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        u32 primitive_count = input.build_range.primitiveCount;
        vkGetAccelerationStructureBuildSizes(ctx.device, 
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &blas_info, &primitive_count, &size_info);

        if (size_info.buildScratchSize > max_scratch_buffer_size)
        {
            max_scratch_buffer_size = size_info.buildScratchSize;
        }
        buffer_size += size_info.accelerationStructureSize;
    }
    LOG_INFO("Acceleration structure size = %ld, Scratch buffer size = %ld", buffer_size, max_scratch_buffer_size);

    // create scratch buffer
    buffer_t scratch_buffer;
    create_buffer(ctx, max_scratch_buffer_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &scratch_buffer);
    const auto scratch_buffer_address = get_buffer_device_address(ctx, scratch_buffer.handle);

    // create buffer for all blas
    buffer_t structure_buffer; 
    create_buffer(ctx, buffer_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, &structure_buffer);

    VkCommandBuffer cmd = allocate_command_buffer(builder.command_allocator);
    VK_CHECK( begin_command_buffer(cmd) );

    VkDeviceSize offset = 0;
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos(blas_infos);
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> p_build_ranges(blas_count);
    for (u32 i = 0; i < blas_count; ++i)
    {
        VkAccelerationStructureCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        create_info.pNext = nullptr;
        create_info.buffer = structure_buffer.handle;
        create_info.size = size_infos[i].accelerationStructureSize;
        create_info.offset = offset;
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR acceleration_structure;
        VK_CHECK( vkCreateAccelerationStructure(ctx.device, &create_info, nullptr, &acceleration_structure) );
        LOG_INFO("Created acceleration structure");
        LOG_INFO("offset %ld, offset (aligned) %ld", offset, (offset + 255) & ~(255));
        offset += size_infos[i].accelerationStructureSize;
        //offset = (offset + 255) & ~(255); // needs to be multiple of 256
       
        auto& build_info = build_infos[i];
        build_info.dstAccelerationStructure = acceleration_structure;
        build_info.scratchData.deviceAddress = scratch_buffer_address;
        p_build_ranges[i] = &builder.inputs[i].build_range;

        //CHECKPOINT(cmd, "(pre) blas");
        vkCmdBuildAccelerationStructures(cmd, 1, &build_info, &p_build_ranges[i]);
        //CHECKPOINT(cmd, "(post) blas");
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    out.resize(blas_count);
    for (u32 i = 0; i < blas_count; ++i) 
    {
        out[i].buffer = structure_buffer;
        out[i].handle = build_infos[i].dstAccelerationStructure;
    }

    end_and_submit_command_buffer(cmd, ctx.q_compute);
    VkResult res = vkQueueWaitIdle(ctx.q_compute);
    if (res != VK_SUCCESS)
    {
        //PRINT_CHECKPOINT_STACK(context->q_compute);
    }
    VK_CHECK( res );
    reset(builder.command_allocator);
    destroy_buffer(ctx, scratch_buffer);
    LOG_INFO("blas creation complete");
}

void destroy_acceleration_structure_builder(acceleration_structure_builder_t& builder)
{
    destroy_staging_buffer(builder.staging);
    destroy_command_allocator(builder.command_allocator);
};


