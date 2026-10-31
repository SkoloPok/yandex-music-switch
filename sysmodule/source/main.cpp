#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <curl/curl.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "api/yandex_daemon.h"
#include "ipc/ipc_server.h"

static volatile bool g_running = true;

#ifndef __SWITCH__
static void handleSignal(int sig) {
    (void)sig;
    g_running = false;
}
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef __SWITCH__
    // Инициализация сервисов Switch
    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);
#else
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    YandexDaemon daemon;
    if (daemon.init()) {
        IpcServer server(&daemon);
        server.start(4539);

        while (g_running) {
#ifdef __SWITCH__
            svcSleepThread(1000000000ULL); // 1 секунда
#else
            std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
        }

        server.stop();
        daemon.shutdown();
    }

    curl_global_cleanup();

#ifdef __SWITCH__
    nifmExit();
    socketExit();
#endif

    return 0;
}
