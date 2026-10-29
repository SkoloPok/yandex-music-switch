// Реализация домашнего экрана

#include "home_screen.h"
#include "../app.h"
#include "theme.h"
#include "../api/auth.h"

HomeScreen::HomeScreen(App* app)
    : Screen(app) {}

void HomeScreen::handleInput(uint64_t kDown, uint64_t /*kHeld*/) {
    if (loading_) return;

    // L/R — переключение вкладок
    if (kDown & HidNpadButton_L) {
        if (activeTab_ > 0) {
            activeTab_--;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
        }
    }
    if (kDown & HidNpadButton_R) {
        if (activeTab_ < 1) {
            activeTab_++;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
            if (activeTab_ == 1 && !likedLoaded_) {
                loadLikedTracks();
            }
        }
    }

    // Определяем размер текущего списка
    int listSize = 0;
    if (activeTab_ == 0) listSize = static_cast<int>(playlists_.size());
    else listSize = static_cast<int>(likedTracks_.size());

    // Навигация вверх/вниз
    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
        if (selectedIndex_ > 0) {
            selectedIndex_--;
            if (selectedIndex_ < scrollOffset_) {
                scrollOffset_ = selectedIndex_;
            }
        }
    }
    if (kDown & (HidNpadButton_Down | HidNpadButton_StickLDown)) {
        if (selectedIndex_ < listSize - 1) {
            selectedIndex_++;
            if (selectedIndex_ >= scrollOffset_ + VISIBLE_ITEMS) {
                scrollOffset_ = selectedIndex_ - VISIBLE_ITEMS + 1;
            }
        }
    }

    // A — выбрать
    if (kDown & HidNpadButton_A) {
        if (activeTab_ == 0 && selectedIndex_ < static_cast<int>(playlists_.size())) {
            // Открыть плейлист
            app_->showPlaylist(playlists_[selectedIndex_]);
        } else if (activeTab_ == 1 && selectedIndex_ < static_cast<int>(likedTracks_.size())) {
            // Воспроизвести трек из избранного
            app_->playPlaylist(likedTracks_, selectedIndex_);
        }
    }

    // X — поиск
    if (kDown & HidNpadButton_X) {
        app_->navigateTo(AppScreen::SEARCH);
    }

    // Y — выход из аккаунта (логаут)
    if (kDown & HidNpadButton_Y) {
        AuthManager::deleteToken();
        playlistsLoaded_ = false;
        likedLoaded_ = false;
        playlists_.clear();
        likedTracks_.clear();
        app_->getPlayer()->stop();
        app_->navigateTo(AppScreen::LOGIN);
    }
}

void HomeScreen::update() {}

void HomeScreen::render() {
    auto* r = app_->getRenderer();

    // Вкладки
    std::vector<std::string> tabs = {"Мои плейлисты", "Мне нравится"};
    r->drawTabBar(tabs, activeTab_);

    int contentY = Theme::TAB_BAR_H;
    int contentH = Theme::SCREEN_H - Theme::TAB_BAR_H - Theme::PLAYER_BAR_H - 35;

    // Ограничиваем область отрисовки
    SDL_Rect clip = {0, contentY, Theme::SCREEN_W, contentH};
    r->setClipRect(&clip);

    if (loading_) {
        r->drawText("Загрузка...", Theme::SCREEN_W / 2, contentY + contentH / 2 - 12,
                     Theme::FONT_NORMAL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
    } else if (activeTab_ == 0) {
        // Плейлисты
        if (playlists_.empty()) {
            r->drawText("Нет плейлистов", Theme::SCREEN_W / 2, contentY + contentH / 2 - 12,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        } else {
            for (int i = scrollOffset_; i < static_cast<int>(playlists_.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
                int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
                bool selected = (i == selectedIndex_);
                std::string subtitle = std::to_string(playlists_[i].trackCount) + " треков";
                r->drawListItem(0, itemY, Theme::SCREEN_W, playlists_[i].title,
                               subtitle, nullptr, selected, false);
            }
        }
    } else if (activeTab_ == 1) {
        // Мне нравится
        if (likedTracks_.empty()) {
            r->drawText("Нет избранных треков", Theme::SCREEN_W / 2, contentY + contentH / 2 - 12,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        } else {
            const Track* currentTrack = app_->getPlayer()->getCurrentTrack();
            for (int i = scrollOffset_; i < static_cast<int>(likedTracks_.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
                int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
                bool selected = (i == selectedIndex_);
                bool playing = currentTrack && currentTrack->id == likedTracks_[i].id;
                r->drawListItem(0, itemY, Theme::SCREEN_W, likedTracks_[i].title,
                               likedTracks_[i].getArtistName(), nullptr, selected, playing);
            }
        }
    }

    r->setClipRect(nullptr);

    // Подсказки кнопок
    std::vector<std::pair<std::string, std::string>> hints = {
        {"A", "Выбрать"}, {"X", "Поиск"}, {"Y", "Выйти"}, {"L/R", "Вкладка"}
    };
    r->drawButtonHints(hints);
}

void HomeScreen::onEnter() {
    if (!playlistsLoaded_) {
        loadPlaylists();
    }
}

void HomeScreen::onExit() {}

void HomeScreen::loadPlaylists() {
    loading_ = true;
    playlists_ = app_->getAPI()->getUserPlaylists();
    playlistsLoaded_ = true;
    loading_ = false;
}

void HomeScreen::loadLikedTracks() {
    loading_ = true;
    likedTracks_ = app_->getAPI()->getLikedTracks();
    likedLoaded_ = true;
    loading_ = false;
}
