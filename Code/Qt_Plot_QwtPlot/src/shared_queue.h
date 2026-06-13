#pragma once

#include <queue>
#include <mutex>

struct Event
{
    double x;
    double y;
};

extern std::queue<Event> g_queue;
extern std::mutex g_mutex;