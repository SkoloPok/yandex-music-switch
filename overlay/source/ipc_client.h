#pragma once

#include <string>

struct OverlayStatus {
    bool connected = false;
    std::string state = "stopped"; // "playing", "paused", "stopped"
    std::string title;
    std::string artist;
    int elapsedSec = 0;
    int durationSec = 0;
    int volume = 85;
    std::string mode = "wave";     // "wave", "liked", "playlist"
};

class IpcClient {
public:
    static OverlayStatus getStatus();
    static bool sendCommand(const std::string& cmd);

private:
    static std::string query(const std::string& request);
};
