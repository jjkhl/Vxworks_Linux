#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

// 数据帧结构
typedef struct SamplerFrame
{
    uint64_t timestampe_ns; // 纳秒时间戳(设备采样时间)
    uint32_t seq;           // 采样序号(检测丢包用)
    float value;            // 采样数据
} SamplerFrame;

// 模拟RTOS时钟
inline uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void sleep_us(uint64_t us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

template <typename T, size_t N>
class RingBuffer
{
public:
    void push(const T &item)
    {
        buffer_[write_] = item;    // 写入当前位置
        write_ = (write_ + 1) % N; // 写指针前进
        if (write_ == read_)       // 写追上读->满了
        {
            read_ = (read_ + 1) % N; // 覆盖旧数据
        }
    }

    bool pop(T &out)
    {
        if (read_ == write_) // 没有数据
            return false;
        out = buffer_[read_];
        read_ = (read_ + 1) % N;
        return true;
    }

private:
    T buffer_[N];
    size_t write_ = 0;
    size_t read_ = 0;
};

static std::atomic<bool> g_running{true};
static RingBuffer<SamplerFrame, 4096> g_ring;

// 模拟硬件+DMA控制器
void collect_thread()
{
    uint32_t seq = 0;
    while (g_running)
    {
        SamplerFrame f;
        f.timestampe_ns = now_ns();
        f.seq = seq++;                   // seq模拟采样
        f.value = std::sin(seq * 0.01f); // 正弦波模拟真实信号

        g_ring.push(f); // DMA写入内存
        sleep_us(1000); // 1kHz = 1000微妙周期
    }
}

void send_thread()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 1);

    std::cout << "Waiting CLient..." << std::endl;
    int client_fd = accept(server_fd, nullptr, nullptr);
    std::cout << "Client connected" << std::endl;

    SamplerFrame frame{};
    while (g_running)
    {
        while (g_ring.pop(frame))
        {
            send(client_fd, &frame, sizeof(frame), 0);
        }

        // 降低CPU占用，模拟低优先级任务
        sleep_us(200);
    }

    close(client_fd);
    close(server_fd);
}

int main()
{
    std::thread t1(collect_thread);
    std::thread t2(send_thread);
    std::cout << "Collector running. Press Enter to exit\n";
    std::cin.get();

    g_running = false;
    t1.join();
    t2.join();
    return 0;
}