// Реализация клиента Yandex Music API
// Все запросы идут через HttpClient к api.music.yandex.net

#include "yandex_api.h"
#include "auth.h"
#include "../utils/xml_parser.h"
#include "../utils/md5.h"

// Мини JSON-парсер (без внешних зависимостей)
// Для простоты используем строковый поиск по JSON
// В продакшене следует использовать nlohmann/json или cJSON

#include <algorithm>
#include <sstream>

const std::string YandexMusicAPI::BASE_URL = "https://api.music.yandex.net";
const std::string YandexMusicAPI::SIGN_SALT = "XGRlBW9FXlekgbPrRHuSiA";

// ============================================================================
// Вспомогательные функции для парсинга JSON (без внешних библиотек)
// ============================================================================

namespace json {

// Извлечь строковое значение по ключу: "key":"value" или "key": "value"
static std::string getString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.length();
    // Пропускаем пробелы и двоеточие
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++; // пропускаем открывающую кавычку
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        return result;
    }
    return "";
}

// Извлечь числовое значение: "key":12345 или "key": 12345
static int64_t getInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;

    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return 0;

    // Может быть число или строка-число
    if (json[pos] == '"') {
        std::string s = getString(json, key);
        try { return std::stoll(s); } catch (...) { return 0; }
    }

    std::string numStr;
    while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '-')) {
        numStr += json[pos++];
    }
    if (numStr.empty()) return 0;
    try { return std::stoll(numStr); } catch (...) { return 0; }
}

// Извлечь булево значение
static bool getBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;

    pos += search.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) pos++;

    return (pos < json.size() && json[pos] == 't');
}

// Извлечь массив JSON объектов: "key":[{...},{...}]
// Возвращает вектор строк — каждый элемент массива как JSON-строка
static std::vector<std::string> getArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;

    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return result;

    pos += search.length();
    while (pos < json.size() && json[pos] != '[') pos++;
    if (pos >= json.size()) return result;

    pos++; // пропускаем '['
    int depth = 0;
    std::string current;
    bool inString = false;

    while (pos < json.size()) {
        char c = json[pos];

        if (c == '"' && (pos == 0 || json[pos-1] != '\\')) {
            inString = !inString;
        }

        if (!inString) {
            if (c == '{' || c == '[') {
                if (c == '{' && depth == 0) {
                    current.clear();
                }
                depth++;
            } else if (c == '}' || c == ']') {
                if (c == ']' && depth == 0) break;
                depth--;
                if (c == '}' && depth == 0) {
                    current += c;
                    result.push_back(current);
                    current.clear();
                    pos++;
                    continue;
                }
            } else if (c == ',' && depth == 0) {
                pos++;
                continue;
            }
        }

        if (depth > 0) {
            current += c;
        }
        pos++;
    }

    return result;
}

// Извлечь вложенный JSON объект: "key":{...}
static std::string getObject(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.length();
    while (pos < json.size() && json[pos] != '{') {
        if (json[pos] == '[' || json[pos] == '"' || json[pos] == ',') return "";
        pos++;
    }
    if (pos >= json.size()) return "";

    int depth = 0;
    size_t start = pos;
    bool inString = false;

    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"' && (pos == 0 || json[pos-1] != '\\')) inString = !inString;
        if (!inString) {
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) return json.substr(start, pos - start + 1);
            }
        }
        pos++;
    }
    return "";
}

} // namespace json

// ============================================================================
// YandexMusicAPI
// ============================================================================

YandexMusicAPI::YandexMusicAPI() {}

void YandexMusicAPI::setToken(const std::string& token) {
    token_ = token;
    http_.setAuthToken(token);
}

bool YandexMusicAPI::isAuthorized() const {
    return !token_.empty() && !userId_.empty();
}

Track YandexMusicAPI::parseTrack(const std::string& item) {
    Track t;
    t.id = std::to_string(json::getInt(item, "id"));
    if (t.id == "0") t.id = json::getString(item, "id");
    t.title = json::getString(item, "title");
    t.durationMs = static_cast<int>(json::getInt(item, "durationMs"));
    t.coverUri = json::getString(item, "coverUri");
    t.available = json::getBool(item, "available");

    // Артисты
    auto artistItems = json::getArray(item, "artists");
    for (const auto& ai : artistItems) {
        Artist a;
        a.id = std::to_string(json::getInt(ai, "id"));
        a.name = json::getString(ai, "name");
        t.artists.push_back(a);
    }

    // Альбомы
    auto albumItems = json::getArray(item, "albums");
    for (const auto& al : albumItems) {
        Album ab;
        ab.id = std::to_string(json::getInt(al, "id"));
        ab.title = json::getString(al, "title");
        ab.coverUri = json::getString(al, "coverUri");
        t.albums.push_back(ab);
    }

    return t;
}

std::vector<Track> YandexMusicAPI::parseTracksArray(const std::string& jsonArray) {
    std::vector<Track> result;
    auto items = json::getArray(jsonArray, "results");
    if (items.empty()) {
        items = json::getArray(jsonArray, "tracks");
    }
    for (const auto& item : items) {
        result.push_back(parseTrack(item));
    }
    return result;
}

std::string YandexMusicAPI::apiGet(const std::string& endpoint) {
    HttpResponse resp = http_.get(BASE_URL + endpoint);
    if (!resp.success) {
        lastError_ = resp.error;
        return "";
    }
    if (resp.statusCode != 200) {
        lastError_ = "HTTP " + std::to_string(resp.statusCode);
        return "";
    }
    return resp.body;
}

std::string YandexMusicAPI::apiPost(const std::string& endpoint, const std::string& data) {
    HttpResponse resp = http_.post(BASE_URL + endpoint, data);
    if (!resp.success) {
        lastError_ = resp.error;
        return "";
    }
    if (resp.statusCode != 200) {
        lastError_ = "HTTP " + std::to_string(resp.statusCode);
        return "";
    }
    return resp.body;
}

// ============================================================================
// Аккаунт
// ============================================================================

bool YandexMusicAPI::getAccountStatus(UserInfo& outInfo) {
    std::string body = apiGet("/account/status");
    if (body.empty()) return false;

    std::string result = json::getObject(body, "result");
    if (result.empty()) return false;

    std::string account = json::getObject(result, "account");
    if (account.empty()) return false;

    outInfo.uid = std::to_string(json::getInt(account, "uid"));
    outInfo.login = json::getString(account, "login");
    outInfo.displayName = json::getString(account, "displayName");
    if (outInfo.displayName.empty()) {
        outInfo.displayName = json::getString(account, "firstName");
    }

    std::string plus = json::getObject(result, "plus");
    outInfo.hasPlus = json::getBool(plus, "hasPlus");

    userId_ = outInfo.uid;
    return true;
}

// ============================================================================
// Поиск
// ============================================================================

SearchResult YandexMusicAPI::search(const std::string& query,
                                    const std::string& type, int page) {
    SearchResult result;

    // URL-encode query (простая версия)
    CURL* curl = curl_easy_init();
    if (!curl) return result;
    char* encoded = curl_easy_escape(curl, query.c_str(), query.length());
    std::string encodedQuery(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    std::string endpoint = "/search?type=" + type + "&text=" + encodedQuery
                         + "&page=" + std::to_string(page) + "&nococrrect=false";

    std::string body = apiGet(endpoint);
    if (body.empty()) return result;

    std::string res = json::getObject(body, "result");
    if (res.empty()) return result;

    // Парсим треки
    std::string tracksObj = json::getObject(res, "tracks");
    if (!tracksObj.empty()) {
        auto trackItems = json::getArray(tracksObj, "results");
        for (const auto& item : trackItems) {
            result.tracks.push_back(parseTrack(item));
        }
    }

    // Парсим альбомы
    std::string albumsObj = json::getObject(res, "albums");
    if (!albumsObj.empty()) {
        auto albumItems = json::getArray(albumsObj, "results");
        for (const auto& item : albumItems) {
            Album a;
            a.id = std::to_string(json::getInt(item, "id"));
            a.title = json::getString(item, "title");
            a.coverUri = json::getString(item, "coverUri");
            a.year = static_cast<int>(json::getInt(item, "year"));
            a.trackCount = static_cast<int>(json::getInt(item, "trackCount"));
            result.albums.push_back(a);
        }
    }

    // Парсим артистов
    std::string artistsObj = json::getObject(res, "artists");
    if (!artistsObj.empty()) {
        auto artistItems = json::getArray(artistsObj, "results");
        for (const auto& item : artistItems) {
            Artist a;
            a.id = std::to_string(json::getInt(item, "id"));
            a.name = json::getString(item, "name");
            a.coverUri = json::getString(item, "coverUri");
            result.artists.push_back(a);
        }
    }

    return result;
}

// ============================================================================
// Плейлисты
// ============================================================================

std::vector<Playlist> YandexMusicAPI::getUserPlaylists() {
    std::vector<Playlist> playlists;
    if (userId_.empty()) return playlists;

    std::string body = apiGet("/users/" + userId_ + "/playlists/list");
    if (body.empty()) return playlists;

    auto items = json::getArray(body, "result");
    for (const auto& item : items) {
        Playlist p;
        p.uid = std::to_string(json::getInt(item, "uid"));
        p.kind = std::to_string(json::getInt(item, "kind"));
        p.title = json::getString(item, "title");
        p.description = json::getString(item, "description");
        p.coverUri = json::getString(item, "coverUri");

        // Попробуем достать coverUri из огimage
        if (p.coverUri.empty()) {
            std::string cover = json::getObject(item, "cover");
            if (!cover.empty()) {
                p.coverUri = json::getString(cover, "uri");
            }
        }

        p.trackCount = static_cast<int>(json::getInt(item, "trackCount"));
        playlists.push_back(p);
    }

    return playlists;
}

Playlist YandexMusicAPI::getPlaylist(const std::string& uid, const std::string& kind) {
    Playlist playlist;

    std::string body = apiGet("/users/" + uid + "/playlists/" + kind);
    if (body.empty()) return playlist;

    std::string result = json::getObject(body, "result");
    if (result.empty()) return playlist;

    playlist.uid = std::to_string(json::getInt(result, "uid"));
    playlist.kind = std::to_string(json::getInt(result, "kind"));
    playlist.title = json::getString(result, "title");
    playlist.description = json::getString(result, "description");
    playlist.trackCount = static_cast<int>(json::getInt(result, "trackCount"));

    // Треки в плейлисте находятся в "tracks"[].track
    auto trackWrappers = json::getArray(result, "tracks");
    for (const auto& wrapper : trackWrappers) {
        std::string trackJson = json::getObject(wrapper, "track");
        if (trackJson.empty()) trackJson = wrapper;
        playlist.tracks.push_back(parseTrack(trackJson));
    }

    return playlist;
}

std::vector<Track> YandexMusicAPI::getAlbumTracks(const std::string& albumId) {
    std::vector<Track> tracks;
    std::string body = apiGet("/albums/" + albumId + "/with-tracks");
    if (body.empty()) return tracks;

    std::string result = json::getObject(body, "result");
    if (result.empty()) return tracks;

    auto volumes = json::getArray(result, "volumes");
    if (!volumes.empty()) {
        for (const auto& volStr : volumes) {
            std::string wrapped = "{\"items\":" + volStr + "}";
            auto trackItems = json::getArray(wrapped, "items");
            for (const auto& item : trackItems) {
                tracks.push_back(parseTrack(item));
            }
        }
    } else {
        auto trackItems = json::getArray(result, "tracks");
        for (const auto& item : trackItems) {
            tracks.push_back(parseTrack(item));
        }
    }

    return tracks;
}

// ============================================================================
// Избранное
// ============================================================================

std::vector<Track> YandexMusicAPI::getLikedTracks() {
    std::vector<Track> tracks;
    if (userId_.empty()) return tracks;

    std::string body = apiGet("/users/" + userId_ + "/likes/tracks");
    if (body.empty()) return tracks;

    std::string result = json::getObject(body, "result");
    std::string library = json::getObject(result, "library");

    auto items = json::getArray(library, "tracks");
    if (items.empty()) {
        items = json::getArray(result, "tracks");
    }

    // Собираем ID треков для batch-запроса
    std::vector<std::string> trackIds;
    for (const auto& item : items) {
        std::string id = json::getString(item, "id");
        if (id.empty()) id = std::to_string(json::getInt(item, "id"));
        if (!id.empty() && id != "0") {
            trackIds.push_back(id);
        }
    }

    if (trackIds.empty()) return tracks;

    // Batch-запрос информации о треках (максимум 50 за раз)
    size_t batchSize = 50;
    for (size_t i = 0; i < trackIds.size() && i < 200; i += batchSize) {
        std::string ids;
        for (size_t j = i; j < trackIds.size() && j < i + batchSize; j++) {
            if (!ids.empty()) ids += ",";
            ids += trackIds[j];
        }

        std::string resp = apiPost("/tracks", "track-ids=" + ids);
        if (resp.empty()) continue;

        auto trackItems = json::getArray(resp, "result");
        for (const auto& tj : trackItems) {
            tracks.push_back(parseTrack(tj));
        }
    }

    return tracks;
}

bool YandexMusicAPI::likeTrack(const std::string& trackId) {
    if (userId_.empty()) return false;
    std::string resp = apiPost("/users/" + userId_ + "/likes/tracks/add",
                               "track-ids=" + trackId);
    return !resp.empty();
}

bool YandexMusicAPI::unlikeTrack(const std::string& trackId) {
    if (userId_.empty()) return false;
    std::string resp = apiPost("/users/" + userId_ + "/likes/tracks/remove",
                               "track-ids=" + trackId);
    return !resp.empty();
}

// ============================================================================
// Загрузка треков (многошаговый процесс)
// ============================================================================

std::string YandexMusicAPI::getTrackDownloadUrl(const std::string& trackId) {
    // Шаг 1: Получить список доступных вариантов загрузки
    std::string body = apiGet("/tracks/" + trackId + "/download-info");
    if (body.empty()) return "";

    auto items = json::getArray(body, "result");
    if (items.empty()) return "";

    // Выбираем лучшее качество (предпочитаем mp3 320, потом 192, потом 128)
    std::string bestUrl;
    int bestBitrate = 0;

    for (const auto& item : items) {
        std::string codec = json::getString(item, "codec");
        int bitrate = static_cast<int>(json::getInt(item, "bitrateInKbps"));
        if (bitrate == 0) bitrate = static_cast<int>(json::getInt(item, "bitrate"));
        std::string downloadInfoUrl = json::getString(item, "downloadInfoUrl");

        if (downloadInfoUrl.empty()) continue;

        // Предпочитаем mp3 с наивысшим битрейтом
        if (codec == "mp3" && bitrate > bestBitrate) {
            bestBitrate = bitrate;
            bestUrl = downloadInfoUrl;
        }
        // Если mp3 нет, берём любой с наивысшим битрейтом
        if (bestUrl.empty() && bitrate > bestBitrate) {
            bestBitrate = bitrate;
            bestUrl = downloadInfoUrl;
        }
    }

    if (bestUrl.empty() && !items.empty()) {
        // Fallback: берём первый доступный
        bestUrl = json::getString(items[0], "downloadInfoUrl");
    }

    if (bestUrl.empty()) {
        lastError_ = "No download URL available";
        return "";
    }

    // Шаг 2: Получить XML с параметрами для формирования финального URL
    HttpResponse xmlResp = http_.get(bestUrl);
    if (!xmlResp.success || xmlResp.body.empty()) {
        lastError_ = "Failed to fetch download info XML";
        return "";
    }

    DownloadInfo info = XmlParser::parseDownloadInfo(xmlResp.body);
    if (info.host.empty() || info.path.empty() || info.s.empty() || info.ts.empty()) {
        lastError_ = "Failed to parse download info XML";
        return "";
    }

    // Шаг 3: Сформировать подпись
    // sign = MD5("XGRlBW9FXlekgbPrRHuSiA" + path[1:] + s)
    std::string pathWithoutSlash = info.path;
    if (!pathWithoutSlash.empty() && pathWithoutSlash[0] == '/') {
        pathWithoutSlash = pathWithoutSlash.substr(1);
    }

    std::string sign = MD5::hash(SIGN_SALT + pathWithoutSlash + info.s);

    // Шаг 4: Собрать финальный URL
    std::string finalUrl = "https://" + info.host + "/get-mp3/" + sign + "/" + info.ts + info.path;

    return finalUrl;
}

std::vector<uint8_t> YandexMusicAPI::downloadTrack(const std::string& trackId) {
    std::string url = getTrackDownloadUrl(trackId);
    if (url.empty()) return {};

    HttpResponse resp = http_.downloadBinary(url);
    if (!resp.success) {
        lastError_ = "Failed to download track: " + resp.error;
        return {};
    }

    return resp.binaryData;
}

std::vector<uint8_t> YandexMusicAPI::downloadCover(const std::string& coverUrl) {
    if (coverUrl.empty()) return {};

    HttpResponse resp = http_.downloadBinary(coverUrl);
    if (!resp.success) return {};

    return resp.binaryData;
}
