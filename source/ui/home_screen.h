#pragma once
// Домашний экран — плейлисты и избранные треки пользователя

#include "screen.h"
#include "../api/types.h"
#include <vector>
#include <string>

class App;

class HomeScreen : public Screen {
public:
    explicit HomeScreen(App* app);

    void handleInput(uint64_t kDown, uint64_t kHeld) override;
    void update() override;
    void render() override;
    void onEnter() override;
    void onExit() override;

private:
    void loadPlaylists();
    void loadLikedTracks();

    int activeTab_ = 0;       // 0 = Плейлисты, 1 = Мне нравится
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;

    std::vector<Playlist> playlists_;
    std::vector<Track> likedTracks_;

    bool playlistsLoaded_ = false;
    bool likedLoaded_ = false;
    bool loading_ = false;

    static constexpr int VISIBLE_ITEMS = 7; // сколько элементов помещается на экран
};
