#ifndef ACCELERATION_STRUCT_H
#define ACCELERATION_STRUCT_H

#include "staging.h"
#include "commands.h"

struct gpu_context_t;

struct acceleration_structure_t
{
    VkAccelerationStructureKHR handle;
    buffer_t buffer;
};

struct acceleration_structure_builder_t
{
    struct blas_input_t
    {
        VkAccelerationStructureGeometryKHR geometry;
        VkAccelerationStructureBuildRangeInfoKHR build_range;
    };

    gpu_context_t* context;
    staging_buffer_t staging; // move to context
    std::vector<blas_input_t> inputs;
    command_allocator_t command_allocator;
};

void init_acceleration_structure_builder(acceleration_structure_builder_t& builder, gpu_context_t* context);
void destroy_acceleration_structure_builder(acceleration_structure_builder_t& builder);
void build_blases(acceleration_structure_builder_t& builder, std::vector<acceleration_structure_t>& blases);
void build_tlas(acceleration_structure_builder_t& builder, std::vector<VkAccelerationStructureInstanceKHR>& instances, acceleration_structure_t& as, bool update = false);

#endif //ACCELERATION_STRUCT_H
