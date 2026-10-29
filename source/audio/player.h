#pragma once
// Аудио плеер на основе SDL2_mixer
// Управляет воспроизведением, очередью треков и прогрессом

#include "../api/types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

enum class PlayerState {
    STOPPED,
    PLAYING,
    PAUSED,
    LOADING
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Инициализация SDL2_mixer (48kHz, стерео)
    bool init();
    void shutdown();

    // Управление воспроизведением
    bool playFromMemory(const std::vector<uint8_t>& audioData);
    void pause();
    void resume();
    void stop();
    void next();
    void previous();

    // Очередь треков
    void setQueue(const std::vector<Track>& tracks, int startIndex = 0);
    void addToQueue(const Track& track);
    void clearQueue();

    // Состояние
    PlayerState getState() const { return state_; }
    const Track* getCurrentTrack() const;
    int getCurrentIndex() const { return currentIndex_; }
    int getQueueSize() const { return static_cast<int>(queue_.size()); }

    // Прогресс воспроизведения
    float getProgress() const;
    int getPositionMs() const;
    int getDurationMs() const;

    // Коллбэки
    using NeedDownloadCallback = std::function<void(const Track& track, int index)>;
    void onNeedDownload(NeedDownloadCallback cb) { needDownloadCb_ = cb; }

    // Вызывать каждый кадр
    void update();

    // Установить состояние (для управления извне)
    void setState(PlayerState state) { state_ = state; }

private:
    PlayerState state_ = PlayerState::STOPPED;
    std::vector<Track> queue_;
    int currentIndex_ = -1;

    Mix_Music* currentMusic_ = nullptr;
    std::vector<uint8_t> currentAudioData_; // данные в памяти пока играет
    SDL_RWops* currentRWops_ = nullptr;

    uint32_t playStartTicks_ = 0;  // SDL_GetTicks() в момент начала воспроизведения
    uint32_t pausedTicks_ = 0;     // накопленное время паузы
    uint32_t pauseStartTicks_ = 0; // когда началась текущая пауза

    NeedDownloadCallback needDownloadCb_;

    void cleanupCurrent();

    // Статический коллбэк окончания трека
    static void onMusicFinished();
    static AudioPlayer* instance_;
};
