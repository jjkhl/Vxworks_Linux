#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <vector>

/*
多个普通优先级生产者线程  ──push──►  MPSC 无锁有界队列  ──pop──►
一个高优先级实时消费者线程 │ ▼ eventfd 通知机制 │ ▼ epoll_wait(0超时) +
sleep_until
生产者：模拟多个外部事件源(传感器、网络、其他线程等)，频率100Hz(10ms一次)
消费者：只有一个高实时优先级线程，目标是每1ms醒来一次，尽量准时，抖动越小越好
通信：使用工业级常见的MPSC无锁环形队列(Muti-Producer Single-Consumer Lock-Free
Queue)+eventfd唤醒组合
*/
// 工业级MPSC有界无锁队列
template <typename T, size_t Capacity>
class MPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    MPSCQueue()
    {
        for (size_t i = 0; i < Capacity; ++i)
        {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // 多生产者
    bool push(const T &data)
    {
        Cell *cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);  // 尝试写入的位置
        for (;;)
        {
            cell = &buffer_[pos & (Capacity - 1)];                        // 等价于pos%Capacity
            size_t seq = cell->sequence.load(std::memory_order_acquire);  // buf_槽位上记录的序列号
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0)
            {
                // 如果pos=pos+1, 则将enqueue_pos_的值更新为pos+1,并返回true
                // 如果pos!=pos+1, 则将pos更新伪enqueue_pos_的当前值，并返回false
                // 综上，下面一行目的是实现多线程下有序的排队写入数据
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)  // 说明队列中的元素还没被消费者消费
                return false;   // full
            else                // 有其他生产者抢先CAS(compare and swap)成功
                pos = enqueue_pos_.load(std::memory_order_release);
        }
        cell->data = data;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // 单消费者
    bool pop(const T &data)
    {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Cell *cell = &buffer_[pos & (Capacity - 1)];
        size_t seq = cell->sequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        // 正好生产者已经写完这个位置
        if (diff == 0)
        {
            dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
            data = cell->data;
            cell->sequence.store(pos + Capacity, std::memory_order_release);
            return true;
        }
        return false;
    }

    void fun()
    {
        std::cout << std::endl;
    }

private:
    typedef struct Cell
    {
        std::atomic<size_t> sequence;
        T data;
    } Cell;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};  // 生产者争抢位置
    alignas(64) std::atomic<size_t> dequeue_pos_{0};  // 消费者独占位置
    alignas(64) Cell buffer_[Capacity];
};

int main()
{
    return 0;
}