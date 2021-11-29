#ifndef TIME_H
#define TIME_H

#include "log.h"
#include "common.h"

#include <chrono>

struct frame_time_t
{
    std::chrono::steady_clock::time_point curr;
    std::chrono::steady_clock::time_point prev;
    f32 delta = 0;
    f32 prev_average_dt = 0;
    f32 average_dt = 0;
    f32 _total_dt = 0;
    u64 _counter = 0;
};

inline u32 get_fps(frame_time_t& time)
{
    u32 fps = 0;
    if (time.delta)
    {
        fps = static_cast<u32>(1.0/time.delta);
    }
    return fps;
}

inline u32 get_average_fps(frame_time_t& time)
{
    u32 fps = 0;
    if (time.average_dt)
    {
        fps = static_cast<u32>(1.0/time.average_dt);
    }
    return fps;
}

inline f32 delta_in_seconds(frame_time_t& time)
{
    return time.delta;
}

inline void update_time(frame_time_t& time, f32 frame_dt = 0.0)
{
    do {
        time.curr = std::chrono::steady_clock::now();
        time.delta = std::chrono::duration_cast<std::chrono::duration<f32>>(time.curr - time.prev).count();
    } while (time.delta < frame_dt);
    time.prev = time.curr;
    time._total_dt += time.delta;
    time._counter++;
    if (time._total_dt >= 0.25)
    {
        time.prev_average_dt = time.average_dt;
        time.average_dt = time._total_dt / time._counter;
        time._total_dt = 0;
        time._counter = 0;
    }
}

inline void print_time_info(frame_time_t& time)
{
    LOG_INFO("\ntimings\n{\n\tfps: %d\n\trenderer_time: %f ms\n\trender_dt: %f ms\n}",
            get_average_fps(time), time.average_dt * 1000.0, (time.average_dt - time.prev_average_dt) * 1000.0);
}

#endif //TIME_H
