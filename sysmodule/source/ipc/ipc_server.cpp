#include "ipc_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>

IpcServer::IpcServer(YandexDaemon* daemon)
    : daemon_(daemon) {}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(int port) {
    stop();
    running_ = true;
    serverThread_ = std::thread(&IpcServer::serverLoop, this, port);
    return true;
}

void IpcServer::stop() {
    if (running_) {
        running_ = false;
        if (serverFd_ >= 0) {
            shutdown(serverFd_, SHUT_RDWR);
            close(serverFd_);
            serverFd_ = -1;
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
}

void IpcServer::serverLoop(int port) {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) return;

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    if (listen(serverFd_, 4) < 0) {
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (!running_) break;
            continue;
        }

        handleClient(clientFd);
        close(clientFd);
    }
}

void IpcServer::handleClient(int clientFd) {
    char buf[1024] = {0};
    ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;

    std::string request(buf, n);
    while (!request.empty() && (request.back() == '\r' || request.back() == '\n' || request.back() == ' ')) {
        request.pop_back();
    }

    std::string response = "OK\n";

    if (request == "STATUS") {
        DaemonStatus st = daemon_->getStatus();
        std::ostringstream ss;
        ss << "STATUS "
           << "state=" << st.state << ";"
           << "title=" << st.title << ";"
           << "artist=" << st.artist << ";"
           << "elapsed=" << st.elapsedSec << ";"
           << "duration=" << st.durationSec << ";"
           << "volume=" << st.volume << ";"
           << "mode=" << st.mode << "\n";
        response = ss.str();
    } else if (request == "PLAY") {
        daemon_->play();
    } else if (request == "PAUSE") {
        daemon_->pause();
    } else if (request == "TOGGLE") {
        daemon_->togglePlay();
    } else if (request == "NEXT") {
        daemon_->nextTrack();
    } else if (request == "PREV") {
        daemon_->prevTrack();
    } else if (request.rfind("VOLUME ", 0) == 0) {
        int vol = std::stoi(request.substr(7));
        daemon_->setVolume(vol);
    } else if (request.rfind("MODE ", 0) == 0) {
        std::string mode = request.substr(5);
        daemon_->setMode(mode);
    } else if (request == "LIKE") {
        daemon_->likeCurrentTrack();
    } else {
        response = "ERR Unknown command\n";
    }

    send(clientFd, response.c_str(), response.size(), 0);
}
