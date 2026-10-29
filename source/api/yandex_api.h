#pragma once
// Клиент Yandex Music API
// Все запросы к api.music.yandex.net — поиск, плейлисты, загрузка треков

#include "types.h"
#include "../utils/http.h"
#include <string>
#include <vector>
#include <functional>

// Информация об аккаунте
struct UserInfo {
    std::string uid;
    std::string login;
    std::string displayName;
    bool hasPlus = false;
};

class YandexMusicAPI {
public:
    YandexMusicAPI();

    // Авторизация
    void setToken(const std::string& token);
    bool isAuthorized() const;

    // Аккаунт: GET /account/status
    // Возвращает true при успешной валидации токена
    bool getAccountStatus(UserInfo& outInfo);

    // Поиск: GET /search?text=...&type=...&page=...
    SearchResult search(const std::string& query,
                        const std::string& type = "all", int page = 0);

    // Плейлисты пользователя: GET /users/{uid}/playlists/list
    std::vector<Playlist> getUserPlaylists();

    // Содержимое плейлиста: GET /users/{uid}/playlists/{kind}
    Playlist getPlaylist(const std::string& uid, const std::string& kind);

    // Треки альбома: GET /albums/{albumId}/with-tracks
    std::vector<Track> getAlbumTracks(const std::string& albumId);

    // Избранные треки: GET /users/{uid}/likes/tracks
    std::vector<Track> getLikedTracks();

    // Лайк/анлайк трека
    bool likeTrack(const std::string& trackId);
    bool unlikeTrack(const std::string& trackId);

    // Получить URL для скачивания трека (многошаговый процесс)
    std::string getTrackDownloadUrl(const std::string& trackId);

    // Скачать аудиоданные трека в память
    std::vector<uint8_t> downloadTrack(const std::string& trackId);

    // Скачать обложку
    std::vector<uint8_t> downloadCover(const std::string& coverUrl);

    std::string getLastError() const { return lastError_; }

private:
    HttpClient http_;
    std::string token_;
    std::string userId_;
    std::string lastError_;

    static const std::string BASE_URL;
    static const std::string SIGN_SALT;

    // Вспомогательные функции
    std::string apiGet(const std::string& endpoint);
    std::string apiPost(const std::string& endpoint, const std::string& data);

    Track parseTrack(const std::string& json);
    std::vector<Track> parseTracksArray(const std::string& jsonArray);
};
