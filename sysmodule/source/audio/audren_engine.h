#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <thread>
#include "mp3_decoder.h"
#include "../stream/ring_buffer.h"

class AudrenEngine {
public:
    AudrenEngine();
    ~AudrenEngine();

    bool init();
    void exit();

    // Запускает воспроизведение потока из переданного RingBuffer
    void playStream(RingBuffer* ringBuffer);

    // Пауза / возобновление
    void setPaused(bool paused);
    bool isPaused() const { return isPaused_; }

    // Остановка
    void stop();
    bool isPlaying() const { return isPlaying_; }

    // Громкость (0 .. 100)
    void setVolume(int volume);
    int getVolume() const { return volume_; }

    // Текущая позиция воспроизведения в секундах
    int getElapsedSeconds() const { return elapsedSeconds_; }

private:
    void audioThreadLoop();

    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<bool> threadRunning_{false};
    std::atomic<int> volume_{85};
    std::atomic<int> elapsedSeconds_{0};

    RingBuffer* currentRingBuffer_{nullptr};
    std::thread audioThread_;

    Mp3Decoder decoder_;

    // Внутренние структуры audren (для Switch)
    void* switchAudioData_{nullptr};
};
