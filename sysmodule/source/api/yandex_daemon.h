#pragma once

#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <thread>
#include "../stream/ring_buffer.h"
#include "../stream/http_streamer.h"
#include "../audio/audren_engine.h"

struct DaemonTrack {
    std::string id;
    std::string title;
    std::string artist;
    int durationSec = 0;
};

struct DaemonStatus {
    std::string state;       // "playing", "paused", "stopped"
    std::string title;
    std::string artist;
    int elapsedSec = 0;
    int durationSec = 0;
    int volume = 85;
    std::string mode = "wave"; // "wave", "liked", "playlist"
    bool isLiked = false;
};

class YandexDaemon {
public:
    YandexDaemon();
    ~YandexDaemon();

    bool init();
    void update();
    void shutdown();

    // Команды управления
    void play();
    void pause();
    void togglePlay();
    void nextTrack();
    void prevTrack();
    void setVolume(int volume);
    void setMode(const std::string& mode);
    void likeCurrentTrack();

    DaemonStatus getStatus();

private:
    std::string loadToken();
    std::string apiGet(const std::string& path);
    std::string apiPost(const std::string& path, const std::string& postData = "");
    std::string getTrackStreamUrl(const std::string& trackId);

    void replenishQueueIfNeeded();
    void fetchMyWaveTracks();
    void fetchLikedTracks();
    void startPlayingTrack(const DaemonTrack& track);
    void playbackWorker();

    std::string token_;
    std::string userId_;
    std::string currentMode_{"wave"};

    AudrenEngine audio_;
    HttpStreamer streamer_;
    RingBuffer ringBuffer_{2 * 1024 * 1024}; // 2 MB

    DaemonTrack currentTrack_{};
    std::deque<DaemonTrack> queue_;
    std::vector<DaemonTrack> history_;
    std::vector<DaemonTrack> likedCache_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> trackChangeRequested_{false};
    std::thread workerThread_;
    std::mutex stateMutex_;

    static const std::string BASE_URL;
    static const std::string SIGN_SALT;
};
