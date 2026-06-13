#ifndef EVENT_H
#define EVENT_H
#include <stdint.h>

typedef struct Event
{
    uint64_t ts;
    int value;
} Event;

#endif