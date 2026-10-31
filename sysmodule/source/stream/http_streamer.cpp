#include "http_streamer.h"
#include <curl/curl.h>

HttpStreamer::HttpStreamer() = default;

HttpStreamer::~HttpStreamer() {
    stop();
}

bool HttpStreamer::start(const std::string& url, RingBuffer* targetBuffer) {
    stop();

    if (url.empty() || !targetBuffer) {
        lastError_ = "Invalid URL or target buffer";
        return false;
    }

    abortRequested_ = false;
    finished_ = false;
    streaming_ = true;
    lastError_.clear();

    workerThread_ = std::thread(&HttpStreamer::streamLoop, this, url, targetBuffer);
    return true;
}

void HttpStreamer::stop() {
    if (streaming_) {
        abortRequested_ = true;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        streaming_ = false;
    }
}

size_t HttpStreamer::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* streamer = static_cast<HttpStreamer*>(userdata);
    if (!streamer || streamer->abortRequested_) {
        return 0; // Прерывает CURL
    }

    size_t totalBytes = size * nmemb;
    return totalBytes;
}

struct StreamContext {
    HttpStreamer* streamer;
    RingBuffer* buffer;
};

static size_t curlWriteToBuffer(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamContext*>(userdata);
    if (!ctx || !ctx->streamer || ctx->streamer->isStreaming() == false) {
        return 0;
    }

    size_t total = size * nmemb;
    size_t written = ctx->buffer->write(reinterpret_cast<const uint8_t*>(ptr), total);
    return written; // Если RingBuffer остановлен, вернётся < total, что прервёт CURL
}

void HttpStreamer::streamLoop(std::string url, RingBuffer* targetBuffer) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        lastError_ = "Failed to initialize CURL";
        streaming_ = false;
        finished_ = true;
        return;
    }

    StreamContext ctx{this, targetBuffer};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Отключаем проверку сертификатов в фоне
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "YandexMusicSwitchSys/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK && !abortRequested_) {
        lastError_ = curl_easy_strerror(res);
    }

    curl_easy_cleanup(curl);

    streaming_ = false;
    finished_ = true;
}
