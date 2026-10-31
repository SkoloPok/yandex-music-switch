#include "yandex_daemon.h"
#include "../utils/md5.h"
#include "../utils/xml_parser.h"
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <random>

const std::string YandexDaemon::BASE_URL = "https://api.music.yandex.net";
const std::string YandexDaemon::SIGN_SALT = "XGRlBW9FXlekgbPrRHuSiA";

namespace json {
    static std::string getString(const std::string& j, const std::string& key) {
        std::string pattern = "\"" + key + "\":\"";
        size_t pos = j.find(pattern);
        if (pos == std::string::npos) {
            pattern = "\"" + key + "\": \"";
            pos = j.find(pattern);
        }
        if (pos == std::string::npos) return "";
        pos += pattern.size();
        size_t end = j.find('"', pos);
        if (end == std::string::npos) return "";
        return j.substr(pos, end - pos);
    }

    static int64_t getInt(const std::string& j, const std::string& key) {
        std::string pattern = "\"" + key + "\":";
        size_t pos = j.find(pattern);
        if (pos == std::string::npos) {
            pattern = "\"" + key + "\": ";
            pos = j.find(pattern);
        }
        if (pos == std::string::npos) return 0;
        pos += pattern.size();
        while (pos < j.size() && (j[pos] == ' ' || j[pos] == '\t')) pos++;
        char* endPtr = nullptr;
        long long val = strtoll(&j[pos], &endPtr, 10);
        return val;
    }

    static std::string getObject(const std::string& j, const std::string& key) {
        std::string pattern = "\"" + key + "\":";
        size_t pos = j.find(pattern);
        if (pos == std::string::npos) {
            pattern = "\"" + key + "\": ";
            pos = j.find(pattern);
        }
        if (pos == std::string::npos) return "";
        pos = j.find('{', pos);
        if (pos == std::string::npos) return "";
        int depth = 0;
        for (size_t i = pos; i < j.size(); ++i) {
            if (j[i] == '{') depth++;
            else if (j[i] == '}') {
                depth--;
                if (depth == 0) return j.substr(pos, i - pos + 1);
            }
        }
        return "";
    }

    static std::vector<std::string> getArray(const std::string& j, const std::string& key) {
        std::vector<std::string> items;
        std::string pattern = "\"" + key + "\":";
        size_t pos = j.find(pattern);
        if (pos == std::string::npos) {
            pattern = "\"" + key + "\": ";
            pos = j.find(pattern);
        }
        if (pos == std::string::npos) return items;
        pos = j.find('[', pos);
        if (pos == std::string::npos) return items;
        pos++;

        int depth = 0;
        size_t itemStart = 0;
        bool inItem = false;
        bool inStr = false;

        for (size_t i = pos; i < j.size(); ++i) {
            char c = j[i];
            if (c == '"' && (i == 0 || j[i - 1] != '\\')) {
                inStr = !inStr;
            }
            if (inStr) continue;

            if (c == '{' || c == '[') {
                if (!inItem) { itemStart = i; inItem = true; }
                depth++;
            } else if (c == '}' || c == ']') {
                if (c == ']' && depth == 0) break;
                depth--;
                if (depth == 0 && inItem) {
                    items.push_back(j.substr(itemStart, i - itemStart + 1));
                    inItem = false;
                }
            } else if (c == ',' && depth == 0) {
                inItem = false;
            }
        }
        return items;
    }
}

YandexDaemon::YandexDaemon() = default;

YandexDaemon::~YandexDaemon() {
    shutdown();
}

std::string YandexDaemon::loadToken() {
    FILE* f = fopen("sdmc:/switch/yandex-music/token.txt", "r");
    if (!f) return "";
    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    std::string tok(buf, n);
    while (!tok.empty() && (tok.back() == '\r' || tok.back() == '\n' || tok.back() == ' ')) {
        tok.pop_back();
    }
    return tok;
}

static size_t curlWriteStr(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string YandexDaemon::apiGet(const std::string& path) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string fullUrl = (path.find("http") == 0) ? path : (BASE_URL + path);

    struct curl_slist* headers = nullptr;
    if (!token_.empty()) {
        std::string auth = "Authorization: OAuth " + token_;
        headers = curl_slist_append(headers, auth.c_str());
    }
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStr);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

std::string YandexDaemon::apiPost(const std::string& path, const std::string& postData) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string fullUrl = (path.find("http") == 0) ? path : (BASE_URL + path);

    struct curl_slist* headers = nullptr;
    if (!token_.empty()) {
        std::string auth = "Authorization: OAuth " + token_;
        headers = curl_slist_append(headers, auth.c_str());
    }
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStr);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

bool YandexDaemon::init() {
    token_ = loadToken();
    if (token_.empty()) {
        return false;
    }

    // Получаем UID пользователя
    std::string statusResp = apiGet("/account/status");
    std::string account = json::getObject(statusResp, "account");
    int64_t uid = json::getInt(account, "uid");
    if (uid > 0) {
        userId_ = std::to_string(uid);
    }

    if (!audio_.init()) {
        return false;
    }

    isRunning_ = true;
    workerThread_ = std::thread(&YandexDaemon::playbackWorker, this);

    return true;
}

void YandexDaemon::shutdown() {
    if (isRunning_) {
        isRunning_ = false;
        trackChangeRequested_ = true;
        ringBuffer_.stop();
        streamer_.stop();
        audio_.stop();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        audio_.exit();
    }
}

void YandexDaemon::play() {
    audio_.setPaused(false);
}

void YandexDaemon::pause() {
    audio_.setPaused(true);
}

void YandexDaemon::togglePlay() {
    audio_.setPaused(!audio_.isPaused());
}

void YandexDaemon::nextTrack() {
    trackChangeRequested_ = true;
    ringBuffer_.stop();
    streamer_.stop();
    audio_.stop();
}

void YandexDaemon::prevTrack() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (history_.size() >= 2) {
        // Берём предыдущий трек из истории
        DaemonTrack prev = history_[history_.size() - 2];
        history_.pop_back();
        queue_.push_front(currentTrack_);
        queue_.push_front(prev);
        trackChangeRequested_ = true;
        ringBuffer_.stop();
        streamer_.stop();
        audio_.stop();
    } else {
        // Перезапускаем текущий трек сначала
        trackChangeRequested_ = true;
        ringBuffer_.stop();
        streamer_.stop();
        audio_.stop();
    }
}

void YandexDaemon::setVolume(int volume) {
    audio_.setVolume(volume);
}

void YandexDaemon::setMode(const std::string& mode) {
    if (currentMode_ == mode) return;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentMode_ = mode;
        queue_.clear();
    }

    nextTrack();
}

void YandexDaemon::likeCurrentTrack() {
    if (userId_.empty() || currentTrack_.id.empty()) return;
    apiPost("/users/" + userId_ + "/likes/tracks/add", "track-ids=" + currentTrack_.id);
}

DaemonStatus YandexDaemon::getStatus() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    DaemonStatus st{};

    if (audio_.isPlaying()) {
        st.state = audio_.isPaused() ? "paused" : "playing";
    } else {
        st.state = "stopped";
    }

    st.title = currentTrack_.title;
    st.artist = currentTrack_.artist;
    st.elapsedSec = audio_.getElapsedSeconds();
    st.durationSec = currentTrack_.durationSec;
    st.volume = audio_.getVolume();
    st.mode = currentMode_;
    st.isLiked = false;

    return st;
}

std::string YandexDaemon::getTrackStreamUrl(const std::string& trackId) {
    std::string body = apiGet("/tracks/" + trackId + "/download-info");
    if (body.empty()) return "";

    auto items = json::getArray(body, "result");
    if (items.empty()) return "";

    std::string bestUrl;
    int bestBitrate = 0;

    for (const auto& it : items) {
        std::string codec = json::getString(it, "codec");
        int bitrate = static_cast<int>(json::getInt(it, "bitrateInKbps"));
        if (bitrate == 0) bitrate = static_cast<int>(json::getInt(it, "bitrate"));
        std::string dlUrl = json::getString(it, "downloadInfoUrl");

        if (codec == "mp3" && bitrate > bestBitrate) {
            bestBitrate = bitrate;
            bestUrl = dlUrl;
        }
    }

    if (bestUrl.empty() && !items.empty()) {
        bestUrl = json::getString(items[0], "downloadInfoUrl");
    }
    if (bestUrl.empty()) return "";

    std::string xml = apiGet(bestUrl);
    if (xml.empty()) return "";

    DownloadInfo info = XmlParser::parseDownloadInfo(xml);
    if (info.host.empty() || info.path.empty() || info.s.empty() || info.ts.empty()) return "";

    std::string pathWithoutSlash = info.path;
    if (!pathWithoutSlash.empty() && pathWithoutSlash[0] == '/') {
        pathWithoutSlash = pathWithoutSlash.substr(1);
    }

    std::string sign = MD5::hash(SIGN_SALT + pathWithoutSlash + info.s);
    return "https://" + info.host + "/get-mp3/" + sign + "/" + info.ts + info.path;
}

void YandexDaemon::fetchMyWaveTracks() {
    std::string resp = apiPost("/rotor/station/user:onyourwave/tracks");
    std::string result = json::getObject(resp, "result");
    auto seq = json::getArray(result, "sequence");

    for (const auto& item : seq) {
        std::string trkStr = json::getObject(item, "track");
        if (trkStr.empty()) continue;

        DaemonTrack t;
        t.id = json::getString(trkStr, "id");
        if (t.id.empty()) t.id = std::to_string(json::getInt(trkStr, "id"));
        t.title = json::getString(trkStr, "title");
        t.durationSec = static_cast<int>(json::getInt(trkStr, "durationMs") / 1000);

        auto artists = json::getArray(trkStr, "artists");
        if (!artists.empty()) {
            t.artist = json::getString(artists[0], "name");
        }

        if (!t.id.empty()) {
            queue_.push_back(t);
        }
    }
}

void YandexDaemon::fetchLikedTracks() {
    if (userId_.empty()) return;

    if (likedCache_.empty()) {
        std::string body = apiGet("/users/" + userId_ + "/likes/tracks");
        std::string result = json::getObject(body, "result");
        std::string library = json::getObject(result, "library");
        auto items = json::getArray(library, "tracks");
        if (items.empty()) items = json::getArray(result, "tracks");

        std::vector<std::string> ids;
        for (const auto& it : items) {
            std::string id = json::getString(it, "id");
            if (id.empty()) id = std::to_string(json::getInt(it, "id"));
            if (!id.empty() && id != "0") ids.push_back(id);
        }

        for (size_t i = 0; i < ids.size() && i < 100; i += 50) {
            std::string batch;
            for (size_t j = i; j < ids.size() && j < i + 50; ++j) {
                if (!batch.empty()) batch += ",";
                batch += ids[j];
            }

            std::string resp = apiPost("/tracks", "track-ids=" + batch);
            auto trackItems = json::getArray(resp, "result");
            for (const auto& tj : trackItems) {
                DaemonTrack t;
                t.id = json::getString(tj, "id");
                if (t.id.empty()) t.id = std::to_string(json::getInt(tj, "id"));
                t.title = json::getString(tj, "title");
                t.durationSec = static_cast<int>(json::getInt(tj, "durationMs") / 1000);
                auto artists = json::getArray(tj, "artists");
                if (!artists.empty()) t.artist = json::getString(artists[0], "name");
                if (!t.id.empty()) likedCache_.push_back(t);
            }
        }
    }

    for (const auto& t : likedCache_) {
        queue_.push_back(t);
    }
}

void YandexDaemon::replenishQueueIfNeeded() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (queue_.size() < 3) {
        if (currentMode_ == "wave") {
            fetchMyWaveTracks();
        } else if (currentMode_ == "liked") {
            fetchLikedTracks();
        }

        // Если не удалось получить треки в "wave", откатываемся к "liked"
        if (queue_.empty() && currentMode_ != "liked") {
            fetchLikedTracks();
        }
    }
}

void YandexDaemon::playbackWorker() {
    while (isRunning_) {
        replenishQueueIfNeeded();

        DaemonTrack trackToPlay{};
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (!queue_.empty()) {
                trackToPlay = queue_.front();
                queue_.pop_front();
                currentTrack_ = trackToPlay;
                history_.push_back(trackToPlay);
            }
        }

        if (trackToPlay.id.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        std::string streamUrl = getTrackStreamUrl(trackToPlay.id);
        if (streamUrl.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        trackChangeRequested_ = false;
        ringBuffer_.reset();
        streamer_.start(streamUrl, &ringBuffer_);
        audio_.playStream(&ringBuffer_);

        // Цикл мониторинга воспроизведения трека
        while (isRunning_ && !trackChangeRequested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            // Если трек подошёл к концу
            if (streamer_.isFinished() && ringBuffer_.empty() && !audio_.isPlaying()) {
                break;
            }

            // Автопереход, если сыграли больше длительности
            if (trackToPlay.durationSec > 0 && audio_.getElapsedSeconds() >= trackToPlay.durationSec + 2) {
                break;
            }
        }

        audio_.stop();
        streamer_.stop();
        ringBuffer_.stop();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
