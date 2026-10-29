#pragma once
// Главный класс приложения — управляет экранами, API и плеером

#include <SDL2/SDL.h>
#include <memory>
#include <vector>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "api/yandex_api.h"
#include "audio/player.h"
#include "ui/renderer.h"

// Перечисление экранов
enum class AppScreen {
    LOGIN,
    HOME,
    SEARCH,
    PLAYLIST
};

// Forward declarations
class Screen;
class LoginScreen;
class HomeScreen;
class SearchScreen;
class PlaylistScreen;
class PlayerBar;
struct Playlist;

class App {
public:
    explicit App(SDL_Renderer* renderer);
    ~App();

    // Инициализация всех систем
    bool init();

    // Главный цикл — вызывается каждый кадр
    void update();
    void render();

    // Навигация между экранами
    void navigateTo(AppScreen screen);
    void goBack();

    // Общие ресурсы (доступ из экранов)
    YandexMusicAPI* getAPI() { return api_.get(); }
    AudioPlayer* getPlayer() { return player_.get(); }
    Renderer* getRenderer() { return renderer_.get(); }

    // Воспроизведение
    void playTrack(const Track& track);
    void playPlaylist(const std::vector<Track>& tracks, int startIndex = 0);

    // Показать плейлист
    void showPlaylist(const Playlist& playlist);

    bool isRunning() const { return running_; }
    void quit() { running_ = false; }

private:
    SDL_Renderer* sdlRenderer_;
    bool running_ = true;

#ifdef __SWITCH__
    PadState pad_state_{};
#endif

    // Системы
    std::unique_ptr<YandexMusicAPI> api_;
    std::unique_ptr<AudioPlayer> player_;
    std::unique_ptr<Renderer> renderer_;

    // Экраны
    AppScreen currentScreen_ = AppScreen::LOGIN;
    std::vector<AppScreen> screenHistory_;

    std::unique_ptr<LoginScreen> loginScreen_;
    std::unique_ptr<HomeScreen> homeScreen_;
    std::unique_ptr<SearchScreen> searchScreen_;
    std::unique_ptr<PlaylistScreen> playlistScreen_;
    std::unique_ptr<PlayerBar> playerBar_;

    Screen* getActiveScreen();
    void handleGlobalInput(uint64_t kDown);
    void downloadAndPlayTrack(const Track& track);
};
