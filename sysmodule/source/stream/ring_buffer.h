#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <algorithm>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity = 2 * 1024 * 1024) // 2 MB по умолчанию
        : capacity_(capacity), buffer_(capacity), head_(0), tail_(0), count_(0), stopped_(false) {}

    ~RingBuffer() {
        stop();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cvRead_.notify_all();
        cvWrite_.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        stopped_ = false;
        cvWrite_.notify_all();
    }

    bool isStopped() const {
        return stopped_;
    }

    size_t write(const uint8_t* data, size_t size) {
        std::unique_lock<std::mutex> lock(mutex_);
        size_t written = 0;

        while (written < size && !stopped_) {
            size_t space = capacity_ - count_;
            if (space == 0) {
                // Буфер полон — ждём, пока читатель освободит место
                cvWrite_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                    return stopped_ || (capacity_ - count_) > 0;
                });
                if (stopped_) break;
                space = capacity_ - count_;
                if (space == 0) continue;
            }

            size_t toWrite = std::min(size - written, space);
            size_t firstChunk = std::min(toWrite, capacity_ - head_);
            std::memcpy(&buffer_[head_], data + written, firstChunk);
            size_t secondChunk = toWrite - firstChunk;
            if (secondChunk > 0) {
                std::memcpy(&buffer_[0], data + written + firstChunk, secondChunk);
            }

            head_ = (head_ + toWrite) % capacity_;
            count_ += toWrite;
            written += toWrite;

            cvRead_.notify_one();
        }

        return written;
    }

    // Читает до size байт из буфера, удаляя прочитанное
    size_t read(uint8_t* dest, size_t size) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ == 0) return 0;

        size_t toRead = std::min(size, count_);
        size_t firstChunk = std::min(toRead, capacity_ - tail_);
        std::memcpy(dest, &buffer_[tail_], firstChunk);
        size_t secondChunk = toRead - firstChunk;
        if (secondChunk > 0) {
            std::memcpy(dest + firstChunk, &buffer_[0], secondChunk);
        }

        tail_ = (tail_ + toRead) % capacity_;
        count_ -= toRead;

        cvWrite_.notify_one();
        return toRead;
    }

    // Подглядывает (peek) до size байт без удаления из буфера
    size_t peek(uint8_t* dest, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return 0;

        size_t toRead = std::min(size, count_);
        size_t firstChunk = std::min(toRead, capacity_ - tail_);
        std::memcpy(dest, &buffer_[tail_], firstChunk);
        size_t secondChunk = toRead - firstChunk;
        if (secondChunk > 0) {
            std::memcpy(dest + firstChunk, &buffer_[0], secondChunk);
        }

        return toRead;
    }

    // Сдвигает указатель чтения (отбрасывает байты после peek)
    void advance(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t toAdvance = std::min(size, count_);
        tail_ = (tail_ + toAdvance) % capacity_;
        count_ -= toAdvance;
        cvWrite_.notify_one();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

private:
    const size_t capacity_;
    std::vector<uint8_t> buffer_;
    size_t head_;
    size_t tail_;
    size_t count_;
    bool stopped_;
    mutable std::mutex mutex_;
    std::condition_variable cvRead_;
    std::condition_variable cvWrite_;
};
