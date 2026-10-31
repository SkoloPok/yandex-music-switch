#pragma once

#include <atomic>
#include <thread>
#include "../api/yandex_daemon.h"

class IpcServer {
public:
    explicit IpcServer(YandexDaemon* daemon);
    ~IpcServer();

    bool start(int port = 4539);
    void stop();

private:
    void serverLoop(int port);
    void handleClient(int clientFd);

    YandexDaemon* daemon_;
    std::atomic<bool> running_{false};
    int serverFd_{-1};
    std::thread serverThread_;
};
