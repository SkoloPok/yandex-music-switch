#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Структуры данных Yandex Music API

struct Artist {
    std::string id;
    std::string name;
    std::string coverUri; // URI обложки артиста
};

struct Album {
    std::string id;
    std::string title;
    std::string coverUri;
    int year = 0;
    int trackCount = 0;
};

struct Track {
    std::string id;
    std::string title;
    std::string coverUri;
    int durationMs = 0;          // Длительность в миллисекундах
    bool available = true;       // Доступен ли для прослушивания
    std::vector<Artist> artists; // Список исполнителей
    std::vector<Album> albums;   // Альбомы (обычно один)

    // Вспомогательные методы
    std::string getArtistName() const {
        if (artists.empty()) return "Unknown Artist";
        std::string result = artists[0].name;
        for (size_t i = 1; i < artists.size(); ++i) {
            result += ", " + artists[i].name;
        }
        return result;
    }

    std::string getAlbumTitle() const {
        if (albums.empty()) return "";
        return albums[0].title;
    }

    std::string getCoverUri(int size = 200) const {
        if (coverUri.empty()) {
            if (!albums.empty() && !albums[0].coverUri.empty()) {
                std::string uri = albums[0].coverUri;
                // Заменяем %%  на размер
                auto pos = uri.find("%%");
                if (pos != std::string::npos) {
                    uri.replace(pos, 2, std::to_string(size) + "x" + std::to_string(size));
                }
                return "https://" + uri;
            }
            return "";
        }
        std::string uri = coverUri;
        auto pos = uri.find("%%");
        if (pos != std::string::npos) {
            uri.replace(pos, 2, std::to_string(size) + "x" + std::to_string(size));
        }
        return "https://" + uri;
    }
};

struct Playlist {
    std::string uid;       // ID владельца
    std::string kind;      // ID плейлиста
    std::string title;
    std::string description;
    std::string coverUri;
    int trackCount = 0;
    std::vector<Track> tracks; // Загруженные треки (могут быть пустыми)

    std::string getCoverUri(int size = 200) const {
        if (coverUri.empty()) return "";
        std::string uri = coverUri;
        auto pos = uri.find("%%");
        if (pos != std::string::npos) {
            uri.replace(pos, 2, std::to_string(size) + "x" + std::to_string(size));
        }
        return "https://" + uri;
    }
};

struct SearchResult {
    std::vector<Track> tracks;
    std::vector<Album> albums;
    std::vector<Artist> artists;
    std::vector<Playlist> playlists;
};

// Опция скачивания трека (битрейт и кодек)
struct TrackDownloadOption {
    std::string url;
    std::string codec;  // "mp3" или "aac"
    int bitrate = 0;
};
