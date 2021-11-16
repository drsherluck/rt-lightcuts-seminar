#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include "backend.h"
#include "log.h"

#include <vector>
#include <cstring>

struct checkpoint_t
{
    checkpoint_t(const char* name, i32 prev = -1) : prev_index(prev)
    {
        strncpy(this->name, name, sizeof(this->name));
        this->name[47] = '\0';
    }

    char name[48];
    i32 prev_index;
};

class checkpoint_tracker_t
{
    SINGLETON(checkpoint_tracker_t)

private:
    std::vector<checkpoint_t> checkpoints;

public:
    void set_checkpoint(VkCommandBuffer cmd, const char* name)
    {
#ifdef VK_DEBUG_CHECKPOINT_NV
        checkpoints.emplace_back(name, checkpoints.size() - 1);
        vkCmdSetCheckpoint(cmd, &checkpoints.back());
#endif 
    }

    void print(VkQueue queue)
    {
#ifdef VK_DEBUG_CHECKPOINT_NV
        u32 checkpoint_count;
        vkGetQueueCheckpointData(queue, &checkpoint_count, nullptr);
        LOG_INFO("Checkpoints = %d, recorded = %d", checkpoint_count, checkpoints.size());
        std::vector<VkCheckpointDataNV> checkpoints(checkpoint_count);
        for (u32 i = 0; i < checkpoint_count; ++i)
        {
            checkpoints[i].sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
        }
        vkGetQueueCheckpointData(queue, &checkpoint_count, checkpoints.data());
        for (u32 i = 0; i < checkpoint_count; i++)
        {
            const char* stage = checkpoints[i].stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT ? "Top of pipe" : "Bottom of pipe";
            LOG_INFO("--- %s ---", stage);

            checkpoint_t* p_data = static_cast<checkpoint_t*>(checkpoints[i].pCheckpointMarker);
            while (p_data)
            {
                LOG_INFO("\t%s", p_data->name);
                if (p_data->prev_index == -1)
                {
                    p_data = nullptr;
                    continue;
                }
                p_data = &this->checkpoints[p_data->prev_index];
            }
        }
#endif
    }
};

#define CHECKPOINT(cmd, name) checkpoint_tracker_t::get_instance().set_checkpoint(cmd, name)
#define PRINT_CHECKPOINT_STACK(queue) checkpoint_tracker_t::get_instance().print(queue)

#endif // CHECKPOINT_H
