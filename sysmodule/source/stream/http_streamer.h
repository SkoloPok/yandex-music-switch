#pragma once

#include "ring_buffer.h"
#include <string>
#include <atomic>
#include <thread>

class HttpStreamer {
public:
    HttpStreamer();
    ~HttpStreamer();

    // Запускает потоковую передачу по HTTPS в переданный кольцевой буфер
    bool start(const std::string& url, RingBuffer* targetBuffer);

    // Останавливает текущее скачивание
    void stop();

    bool isStreaming() const { return streaming_; }
    bool isFinished() const { return finished_; }
    std::string getLastError() const { return lastError_; }

private:
    void streamLoop(std::string url, RingBuffer* targetBuffer);
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::atomic<bool> streaming_{false};
    std::atomic<bool> abortRequested_{false};
    std::atomic<bool> finished_{false};
    std::thread workerThread_;
    std::string lastError_;
};
