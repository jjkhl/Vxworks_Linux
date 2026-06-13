#include "Qt_Plot.h"
#include <QApplication>
#include <thread>
#include "rt_client.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Qt_Plot w;
    std::thread(rpc_recv_thread).detach();
    w.show();
    return a.exec();
}