#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#define CHANNEL_NUM 4

struct Event
{
    double x;
    // double y;
    double y[CHANNEL_NUM];
};

//////////////////////////////////////////////////
// 客户端列表
//////////////////////////////////////////////////

std::vector<int> clients;

std::mutex clientsMutex;

double random_y(double x)
{
    static std::mt19937 gen(std::random_device{}());
    // 高斯噪声, 平均值0.0：不会让整体函数向上或向下便宜；标准差0.5：数值越大，波动越剧烈
    static std::normal_distribution<double> noise(0.0, 0.5);
    return sin(x) + noise(gen);
}

//////////////////////////////////////////////////
// 广播线程
//////////////////////////////////////////////////

void broadcastThread()
{
    double x = 0;

    while (true)
    {
        x += 0.05;

        Event e;

        e.x = x;

        // e.y = 5 * std::sin(x) + ((rand() % 100) / 100.0 - 0.5);
        e.y[0] = std::sin(x);
        e.y[1] = std::cos(x);
        e.y[2] = std::sin(x * 0.5) * 3;
        e.y[3] = std::cos(x * 2.0) * 2;
        // std::cout << e.y << std::endl;

        //////////////////////////////////////////////////
        // 广播给所有客户端
        //////////////////////////////////////////////////
        {
            std::lock_guard<std::mutex> lock(clientsMutex);

            for (auto it = clients.begin(); it != clients.end();)
            {
                int fd = *it;

                int n = send(fd, &e, sizeof(e), MSG_NOSIGNAL);

                //////////////////////////////////////////////////
                // 客户端断开
                //////////////////////////////////////////////////
                if (n <= 0)
                {
                    std::cout << "client disconnected\n";

                    close(fd);

                    it = clients.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

//////////////////////////////////////////////////
// main
//////////////////////////////////////////////////

int main()
{
    //////////////////////////////////////////////////
    // 忽略SIGPIPE
    //////////////////////////////////////////////////
    signal(SIGPIPE, SIG_IGN);

    int serverfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;

    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};

    addr.sin_family = AF_INET;

    addr.sin_port = htons(12345);

    addr.sin_addr.s_addr = INADDR_ANY;

    bind(serverfd, (sockaddr*)&addr, sizeof(addr));

    listen(serverfd, 10);

    //////////////////////////////////////////////////
    // 广播线程
    //////////////////////////////////////////////////
    std::thread(broadcastThread).detach();

    std::cout << "server start...\n";

    //////////////////////////////////////////////////
    // accept循环
    //////////////////////////////////////////////////
    while (true)
    {
        int clientfd = accept(serverfd, nullptr, nullptr);

        if (clientfd < 0)
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);

            clients.push_back(clientfd);
        }

        std::cout << "client connected\n";
    }
}