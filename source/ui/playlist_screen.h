#pragma once
// Экран плейлиста — отображение треков плейлиста/альбома

#include "screen.h"
#include "../api/types.h"
#include <vector>
#include <string>

class App;

class PlaylistScreen : public Screen {
public:
    explicit PlaylistScreen(App* app);

    void handleInput(uint64_t kDown, uint64_t kHeld) override;
    void update() override;
    void render() override;
    void onEnter() override;
    void onExit() override;

    // Установить плейлист для отображения
    void setPlaylist(const Playlist& playlist);

private:
    void loadTracks();
    void downloadSelectedTrack();
    void downloadAllTracks();

    Playlist playlist_;
    bool tracksLoaded_ = false;
    bool loading_ = false;

    int selectedIndex_ = 0;
    int scrollOffset_ = 0;

    std::string statusMessage_;
    int statusTimer_ = 0;
    bool isDownloading_ = false;

    static constexpr int VISIBLE_ITEMS = 6; // меньше из-за заголовка
};
