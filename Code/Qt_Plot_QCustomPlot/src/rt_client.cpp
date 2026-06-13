#include "rt_client.h"
#include "shared_queue.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

std::queue<Event> g_queue;
std::mutex g_mutex;

void rpc_recv_thread()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);

    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cout << "connect failed\n";
        return;
    }

    while (true)
    {
        Event e;

        int n = read(sock, &e, sizeof(e));

        if (n <= 0)
            break;

        {
            std::lock_guard<std::mutex> lock(g_mutex);

            g_queue.push(e);
        }
    }
}