#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <sys/mman.h>

//////////////////////////////////////////////////////////////
// MPMC 无锁队列（Vyukov）
//////////////////////////////////////////////////////////////
template <typename T, size_t Capacity>
class MPMCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0);

public:
    MPMCQueue()
    {
        for (size_t i = 0; i < Capacity; i++)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool push(const T &data)
    {
        Cell *cell;
        size_t pos = enqueue_pos.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &buffer_[pos & mask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0)
            {
                if (enqueue_pos.compare_exchange_weak(pos, pos + 1))
                    break;
            }
            else if (diff < 0)
            {
                return false;  // full
            }
            else
            {
                pos = enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        cell->data = data;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T &data)
    {
        Cell *cell;
        size_t pos = dequeue_pos.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &buffer_[pos & mask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

            if (diff == 0)
            {
                if (dequeue_pos.compare_exchange_weak(pos, pos + 1))
                    break;
            }
            else if (diff < 0)
            {
                return false;
            }
            else
            {
                pos = dequeue_pos.load(std::memory_order_relaxed);
            }
        }

        data = cell->data;
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return true;
    }

private:
    struct Cell
    {
        std::atomic<size_t> sequence;
        T data;
    };

    static constexpr size_t mask = Capacity - 1;

    alignas(64) std::atomic<size_t> enqueue_pos{0};
    alignas(64) std::atomic<size_t> dequeue_pos{0};
    alignas(64) Cell buffer_[Capacity];
};

//////////////////////////////////////////////////////////////
// 数据结构
//////////////////////////////////////////////////////////////
struct Event
{
    uint64_t ts;
    int value;
};

MPMCQueue<Event, 1024> queue;

//////////////////////////////////////////////////////////////
// 设置实时优先级
//////////////////////////////////////////////////////////////
void set_realtime_priority(int prio)
{
    sched_param param{};
    param.sched_priority = prio;

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param))
    {
        perror("sched_setscheduler");
    }
}

//////////////////////////////////////////////////////////////
// Jitter统计
//////////////////////////////////////////////////////////////
class JitterStats
{
public:
    void record(int64_t ns)
    {
        if (ns < min)
            min = ns;
        if (ns > max)
            max = ns;
        sum += ns;
        count++;
    }

    void print()
    {
        if (count == 0)
            return;

        std::cout << "[Jitter ns] min=" << min << " max=" << max << " avg=" << sum / count << "\n";
    }

private:
    int64_t min = INT64_MAX;
    int64_t max = 0;
    int64_t sum = 0;
    int64_t count = 0;
};

//////////////////////////////////////////////////////////////
// timerfd 创建
//////////////////////////////////////////////////////////////
int create_timerfd(int interval_ms)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    itimerspec its{};
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = interval_ms * 1000000;
    its.it_value = its.it_interval;

    timerfd_settime(tfd, 0, &its, nullptr);

    return tfd;
}

//////////////////////////////////////////////////////////////
// Producer（模拟ISR源）
//////////////////////////////////////////////////////////////
void producer(int id, int efd)
{
    while (true)
    {
        Event e;
        e.ts = std::chrono::steady_clock::now().time_since_epoch().count();
        e.value = id;

        while (!queue.push(e))
        {
            // 可改为丢弃策略
        }

        uint64_t one = 1;
        write(efd, &one, sizeof(one));

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

//////////////////////////////////////////////////////////////
// Consumer（RT线程）
//////////////////////////////////////////////////////////////
void consumer_rt(int efd)
{
    set_realtime_priority(80);

    // 锁内存（防止page fault）
    mlockall(MCL_CURRENT | MCL_FUTURE);

    int ep = epoll_create1(0);

    // eventfd（数据事件）
    epoll_event ev1{};
    ev1.events = EPOLLIN;
    ev1.data.u32 = 1;
    epoll_ctl(ep, EPOLL_CTL_ADD, efd, &ev1);

    // timerfd（1ms tick）
    int tfd = create_timerfd(1);

    epoll_event ev2{};
    ev2.events = EPOLLIN;
    ev2.data.u32 = 2;
    epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &ev2);

    epoll_event events[2];

    JitterStats jitter;
    auto last = std::chrono::steady_clock::now();

    int counter = 0;

    while (true)
    {
        int n = epoll_wait(ep, events, 2, -1);

        for (int i = 0; i < n; i++)
        {
            // ===== tick =====
            if (events[i].data.u32 == 2)
            {
                uint64_t exp;
                read(tfd, &exp, sizeof(exp));

                auto now = std::chrono::steady_clock::now();

                auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count();

                jitter.record(diff);
                last = now;

                if (++counter % 5000 == 0)
                    jitter.print();
            }

            // ===== data =====
            else if (events[i].data.u32 == 1)
            {
                uint64_t cnt;
                read(efd, &cnt, sizeof(cnt));

                Event e;
                while (queue.pop(e))
                {
                    // 实际处理逻辑
                    // printf("consume: %d\n", e.value);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////
// main
//////////////////////////////////////////////////////////////
int main()
{
    int efd = eventfd(0, EFD_NONBLOCK);

    std::vector<std::thread> producers;

    for (int i = 0; i < 4; i++)
        producers.emplace_back(producer, i, efd);

    std::thread consumer(consumer_rt, efd);

    for (auto &t : producers)
        t.join();

    consumer.join();
}