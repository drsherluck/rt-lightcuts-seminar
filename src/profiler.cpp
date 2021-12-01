#include "profiler.h"

void init_profiler(VkDevice device, profiler_t& profiler)
{
    profiler.device = device;
    profiler.query_count = 2;
    VkQueryPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = profiler.query_count; // I think 2 should be enough (reset every begin timer)

    VK_CHECK( vkCreateQueryPool(device, &info, nullptr, &profiler.query_pool) );
}

void destroy_profiler(profiler_t& profiler)
{
    vkDestroyQueryPool(profiler.device, profiler.query_pool, nullptr);
}

void begin_timer(profiler_t& profiler, VkCommandBuffer cmd)
{
    vkCmdResetQueryPool(cmd, profiler.query_pool, 0, profiler.query_count);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, profiler.query_pool, 0);
}

void end_timer(profiler_t& profiler, VkCommandBuffer cmd)
{
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, profiler.query_pool, 1);
}

f64 get_results(profiler_t& profiler)
{
    u64 data[2];
    // todo do not wait
    VK_CHECK( vkGetQueryPoolResults(profiler.device, profiler.query_pool, 0, profiler.query_count, 
                sizeof(u64)*2, data, sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) );
    u64 duration = data[1] - data[0];
    return static_cast<f64>(duration) * 0.000001;
}

