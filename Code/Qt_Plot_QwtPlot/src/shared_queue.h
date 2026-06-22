#pragma once

#include <queue>
#include <mutex>

constexpr int CHANNEL_NUM = 4;

struct Event
{
    double x;
    double y[CHANNEL_NUM];
};

extern std::queue<Event> g_queue;
extern std::mutex g_mutex;