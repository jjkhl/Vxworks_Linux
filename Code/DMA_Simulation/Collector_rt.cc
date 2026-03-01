#include <iostream>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sched.h>
#include <time.h>

#include <vector>
#include <fcntl.h>
#include <sys/epoll.h>

struct SampleFrame
{
    uint64_t timestamp_ns;
    uint32_t seq;
    float value;
};

static std::atomic<bool> g_running{true};

inline uint64_t now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

template <typename T, size_t N>
class RingBuffer
{
public:
    bool push(const T &item) // 消费者
    {
        size_t w = write_.load(std::memory_order_relaxed);
        size_t next = (w + 1) % N;

        if (next == read_.load(std::memory_order_acquire))
        {
            dropped_++;
            return false; // 满了 → 丢弃
        }

        buffer_[w] = item;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T &out) // 生产者
    {
        size_t r = read_.load(std::memory_order_relaxed);

        if (r == write_.load(std::memory_order_acquire))
            return false;

        out = buffer_[r];
        read_.store((r + 1) % N, std::memory_order_release);
        return true;
    }

    uint64_t dropped() const { return dropped_; }

private:
    alignas(64) std::atomic<size_t> write_{0}; // 按照64字节对齐，现代 CPU cache line = 64 bytes。避免cache line伪共享
    alignas(64) std::atomic<size_t> read_{0};
    alignas(64) std::atomic<uint64_t> dropped_{0};
    T buffer_[N];
};

void set_realtime_priority(int priority)
{
    sched_param param;
    param.sched_priority = priority; // 数字越大优先级越高

    if (pthread_setschedparam(
            pthread_self(),
            SCHED_FIFO, // Linux实时调度策略
            &param) != 0)
    {
        perror("Failed to set RT priority");
    }
}

static RingBuffer<SampleFrame, 4096> g_ring;

void collect_thread()
{
    set_realtime_priority(80); // 高优先级

    const uint64_t period_ns = 1000000; // 1kHz
    uint64_t next_time = now_ns();
    uint32_t seq = 0;

    uint64_t max_jitter = 0;

    while (g_running)
    {

        next_time += period_ns;

        timespec ts;
        ts.tv_sec = next_time / 1000000000ULL;
        ts.tv_nsec = next_time % 1000000000ULL;

        clock_nanosleep(CLOCK_MONOTONIC, // 单调递增时钟，不受系统时间调整印象
                        TIMER_ABSTIME,   // 绝对时间
                        &ts,
                        nullptr);

        uint64_t actual = now_ns();
        // 统计调度抖动
        uint64_t jitter = (actual > next_time)
                              ? actual - next_time
                              : next_time - actual;

        if (jitter > max_jitter) // 统计最大抖动
            max_jitter = jitter;

        // 模拟采样数据
        SampleFrame f;
        f.timestamp_ns = actual;
        f.seq = seq++;
        f.value = std::sin(seq * 0.01f);

        g_ring.push(f);

        if (seq % 1000 == 0)
        {
            std::cout << "[RT] Max jitter(ns): "
                      << max_jitter << "\n";
            max_jitter = 0;
        }
    }
}

// 采集线程写入RingBuffer的数据，通过TCP socket发送给外部客户端
void send_thread()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000); // 端口
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr *)&addr, sizeof(addr)); // 0.0.0.0:9000
    listen(server_fd, 1);                             // 1个连接等待

    std::cout << "Waiting client...\n";
    int client_fd = accept(server_fd, nullptr, nullptr); // 阻塞等待Qt客户端
    std::cout << "Client connected\n";

    SampleFrame frame;

    while (g_running)
    {
        while (g_ring.pop(frame))
        {
            ssize_t ret = send(client_fd, &frame, sizeof(frame), 0);
        }
        usleep(200);
    }

    close(client_fd);
    close(server_fd);
}

// epoll版本
void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK); // 设置非阻塞
}

void send_thread_epoll()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr *)&addr, sizeof(addr));
    listen(server_fd, SOMAXCONN);

    set_nonblocking(server_fd);

    int epfd = epoll_create1(0);
    epoll_event ev{};

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    std::vector<int> clients;
    epoll_event events[32];

    SampleFrame frame;

    while (g_running)
    {
        int nfds = epoll_wait(epfd, events, 32, 10);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // 新客户端连接
                int client_fd = accept(server_fd, nullptr, nullptr);
                set_nonblocking(client_fd);

                epoll_event cev{};
                cev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                cev.data.fd = client_fd;

                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                clients.push_back(client_fd);

                std::cout << "Client connected" << std::endl;
            }
            else
            {
                int client_fd = events[i].data.fd;
                if (events[i].events & EPOLLOUT)
                {
                    while (g_ring.pop(frame))
                    {
                        ssize_t ret = send(client_fd, &frame, sizeof(frame), MSG_NOSIGNAL);
                        if (ret <= 0)
                        {
                            close(client_fd);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                            break;
                        }
                    }
                }

                if (events[i].events & (EPOLLHUP | EPOLLERR))
                {
                    close(client_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                }
            }
        }
    }

    close(server_fd);
    close(epfd);
}

int main()
{
    std::thread t1(collect_thread);
    std::thread t2(send_thread);

    std::cout << "RT Collector running. Press Enter to exit.\n";
    std::cin.get();

    g_running = false;

    t1.join();
    t2.join();
    return 0;
}