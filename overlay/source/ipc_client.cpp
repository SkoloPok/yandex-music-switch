#include "ipc_client.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>

std::string IpcClient::query(const std::string& request) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";

    // Устанавливаем таймаут соединения 500мс
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(4539);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return "";
    }

    std::string toSend = request + "\n";
    send(fd, toSend.c_str(), toSend.size(), 0);

    char buf[1024] = {0};
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    close(fd);

    if (n <= 0) return "";
    return std::string(buf, n);
}

static int safeStoi(const std::string& str, int defVal = 0) {
    if (str.empty()) return defVal;
    char* end = nullptr;
    long v = strtol(str.c_str(), &end, 10);
    return (end != str.c_str()) ? static_cast<int>(v) : defVal;
}

OverlayStatus IpcClient::getStatus() {
    OverlayStatus st{};
    std::string resp = query("STATUS");
    if (resp.empty()) {
        st.connected = false;
        return st;
    }

    st.connected = true;

    // Формат ответа: STATUS state=...;title=...;artist=...;elapsed=...;duration=...;volume=...;mode=...
    if (resp.rfind("STATUS ", 0) == 0) {
        std::string payload = resp.substr(7);
        std::istringstream ss(payload);
        std::string token;

        while (std::getline(ss, token, ';')) {
            size_t eq = token.find('=');
            if (eq == std::string::npos) continue;

            std::string key = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) {
                val.pop_back();
            }

            if (key == "state") st.state = val;
            else if (key == "title") st.title = val;
            else if (key == "artist") st.artist = val;
            else if (key == "elapsed") st.elapsedSec = safeStoi(val);
            else if (key == "duration") st.durationSec = safeStoi(val);
            else if (key == "volume") st.volume = safeStoi(val, 85);
            else if (key == "mode") st.mode = val;
        }
    }

    return st;
}

bool IpcClient::sendCommand(const std::string& cmd) {
    std::string resp = query(cmd);
    return !resp.empty();
}
