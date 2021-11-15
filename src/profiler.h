#ifndef PROFILE_H
#define PROFILE_H

#include "backend.h"

struct profiler_t
{
    VkDevice device;
    VkQueryPool query_pool;
    u32 query_count;
};

void init_profiler(VkDevice device, profiler_t& profiler);
void destroy_profiler(profiler_t& profiler);
void begin_timer(profiler_t& profiler, VkCommandBuffer cmd);

// return ms
void end_timer(profiler_t& profiler, VkCommandBuffer cmd);
f64 get_results(profiler_t& profiler);

#endif // PROFILE_H
