#ifndef TIME_H
#define TIME_H

#include "log.h"
#include "common.h"

#include <ctime>

struct frame_time_t
{
    clock_t curr            = 0;
    clock_t prev            = 0;
    clock_t delta           = 0;
    clock_t prev_average_dt = 0;
    clock_t average_dt      = 0;
    clock_t _total_dt       = 0;
    u64 _counter            = 0;
};

inline clock_t get_fps(frame_time_t& time)
{
    clock_t fps = 0;
    if (time.delta)
    {
        fps = CLOCKS_PER_SEC/time.delta;
    }
    return fps;
}

inline clock_t get_average_fps(frame_time_t& time)
{
    clock_t fps = 0;
    if (time.delta)
    {
        fps = CLOCKS_PER_SEC/time.average_dt;
    }
    return fps;
}

inline f32 delta_in_seconds(frame_time_t& time)
{
    return time.delta/static_cast<f32>(CLOCKS_PER_SEC);
}

inline void update_time(frame_time_t& time)
{
    time.curr = clock();
    time.delta = time.curr - time.prev;
    time._total_dt += time.delta;
    time._counter++;
    if (time._total_dt >= CLOCKS_PER_SEC * 0.25)
    {
        time.prev_average_dt = time.average_dt;
        time.average_dt = time._total_dt / time._counter;
        time._total_dt  = 0;
        time._counter   = 0;
    }
}

inline void print_time_info(frame_time_t& time)
{
    LOG_INFO("\ntimings\n{\n\tfps: %d\n\trenderer_time: %f ms\n\trender_dt: %f ms\n}",
            get_average_fps(time), 
            (time.average_dt * 1000.0)/static_cast<f32>(CLOCKS_PER_SEC),
            ((time.average_dt - time.prev_average_dt) * 1000.0)/static_cast<f32>(CLOCKS_PER_SEC));
}

#endif //TIME_H
