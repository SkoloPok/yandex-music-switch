// Реализация экрана поиска

#include "search_screen.h"
#include "../app.h"
#include "theme.h"
#include "keyboard.h"

SearchScreen::SearchScreen(App* app)
    : Screen(app) {}

void SearchScreen::handleInput(uint64_t kDown, uint64_t /*kHeld*/) {
    if (loading_) return;

    // B — назад
    if (kDown & HidNpadButton_B) {
        app_->goBack();
        return;
    }

    // X — новый поиск
    if (kDown & HidNpadButton_X) {
        performSearch();
        return;
    }

    // L/R — переключение вкладок результатов
    if (kDown & HidNpadButton_L) {
        if (activeTab_ > 0) {
            activeTab_--;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
        }
    }
    if (kDown & HidNpadButton_R) {
        if (activeTab_ < 2) {
            activeTab_++;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
        }
    }

    // Размер текущего списка
    int listSize = 0;
    if (activeTab_ == 0) listSize = static_cast<int>(results_.tracks.size());
    else if (activeTab_ == 1) listSize = static_cast<int>(results_.albums.size());
    else listSize = static_cast<int>(results_.artists.size());

    // Навигация
    if (kDown & (HidNpadButton_Up | HidNpadButton_StickLUp)) {
        if (selectedIndex_ > 0) {
            selectedIndex_--;
            if (selectedIndex_ < scrollOffset_) scrollOffset_ = selectedIndex_;
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
        if (activeTab_ == 0 && selectedIndex_ < static_cast<int>(results_.tracks.size())) {
            // Воспроизвести трек
            app_->playPlaylist(results_.tracks, selectedIndex_);
        } else if (activeTab_ == 1 && selectedIndex_ < static_cast<int>(results_.albums.size())) {
            // Открыть альбом как плейлист
            Playlist p;
            p.title = results_.albums[selectedIndex_].title;
            p.uid = "";
            p.kind = results_.albums[selectedIndex_].id;
            p.trackCount = results_.albums[selectedIndex_].trackCount;
            app_->showPlaylist(p);
        } else if (activeTab_ == 2 && selectedIndex_ < static_cast<int>(results_.artists.size())) {
            // Поиск по выбранному артисту
            query_ = results_.artists[selectedIndex_].name;
            loading_ = true;
            results_ = app_->getAPI()->search(query_);
            hasResults_ = !results_.tracks.empty() || !results_.albums.empty() || !results_.artists.empty();
            loading_ = false;
            activeTab_ = 0;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
        }
    }

    // Y — лайк трека
    if (kDown & HidNpadButton_Y) {
        if (activeTab_ == 0 && selectedIndex_ < static_cast<int>(results_.tracks.size())) {
            app_->getAPI()->likeTrack(results_.tracks[selectedIndex_].id);
        }
    }

    // ZR — скачать трек
    if ((kDown & HidNpadButton_ZR) && activeTab_ == 0 && selectedIndex_ < static_cast<int>(results_.tracks.size())) {
        const auto& track = results_.tracks[selectedIndex_];
        statusMessage_ = "Скачивание: " + track.title + "...";
        isDownloading_ = true;
        app_->render();
        bool ok = app_->getAPI()->downloadTrackToFile(track);
        if (ok) {
            statusMessage_ = "Сохранено в /music: " + track.title;
        } else {
            statusMessage_ = "Ошибка: " + app_->getAPI()->getLastError();
        }
        statusTimer_ = 180;
        isDownloading_ = false;
    }
}

void SearchScreen::update() {
    if (statusTimer_ > 0) {
        statusTimer_--;
        if (statusTimer_ == 0 && !isDownloading_) {
            statusMessage_.clear();
        }
    }
}

void SearchScreen::render() {
    auto* r = app_->getRenderer();

    // Поисковая строка
    r->drawRect(0, 0, Theme::SCREEN_W, 46, Theme::BG_SECONDARY);
    if (query_.empty()) {
        r->drawText("Нажмите X для поиска", Theme::SCREEN_W / 2, 12,
                     Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
    } else {
        r->drawText("🔍 " + query_, Theme::PADDING, 12,
                     Theme::FONT_NORMAL, Theme::TEXT_PRIMARY, TextAlign::LEFT);
    }

    if (!hasResults_) {
        if (loading_) {
            r->drawText("Поиск...", Theme::SCREEN_W / 2, Theme::SCREEN_H / 2 - 12,
                         Theme::FONT_NORMAL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
        } else if (!query_.empty()) {
            r->drawText("Нет результатов", Theme::SCREEN_W / 2, Theme::SCREEN_H / 2 - 12,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        }
        // Подсказки
        std::vector<std::pair<std::string, std::string>> hints = {
            {"X", "Поиск"}, {"B", "Назад"}
        };
        r->drawButtonHints(hints);
        return;
    }

    // Вкладки результатов
    int tabY = 46;
    std::vector<std::string> tabs = {"Треки", "Альбомы", "Артисты"};
    r->drawTabBar(tabs, activeTab_, tabY);

    int contentY = tabY + Theme::TAB_BAR_H;
    int contentH = Theme::SCREEN_H - contentY - Theme::PLAYER_BAR_H - 35;
    SDL_Rect clip = {0, contentY, Theme::SCREEN_W, contentH};
    r->setClipRect(&clip);

    const Track* currentTrack = app_->getPlayer()->getCurrentTrack();

    if (activeTab_ == 0) {
        // Треки
        if (results_.tracks.empty()) {
            r->drawText("Нет треков", Theme::SCREEN_W / 2, contentY + 50,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        }
        for (int i = scrollOffset_; i < static_cast<int>(results_.tracks.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
            int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
            bool selected = (i == selectedIndex_);
            bool playing = currentTrack && currentTrack->id == results_.tracks[i].id;
            bool isDownloaded = app_->getAPI()->isTrackDownloaded(results_.tracks[i]);
            std::string dlTag = isDownloaded ? " [SD]" : "";
            r->drawListItem(0, itemY, Theme::SCREEN_W, results_.tracks[i].title + dlTag,
                           results_.tracks[i].getArtistName(), nullptr, selected, playing);
        }
    } else if (activeTab_ == 1) {
        // Альбомы
        if (results_.albums.empty()) {
            r->drawText("Нет альбомов", Theme::SCREEN_W / 2, contentY + 50,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        }
        for (int i = scrollOffset_; i < static_cast<int>(results_.albums.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
            int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
            bool selected = (i == selectedIndex_);
            std::string subtitle = std::to_string(results_.albums[i].year);
            if (results_.albums[i].trackCount > 0) {
                subtitle += " • " + std::to_string(results_.albums[i].trackCount) + " треков";
            }
            r->drawListItem(0, itemY, Theme::SCREEN_W, results_.albums[i].title,
                           subtitle, nullptr, selected, false);
        }
    } else if (activeTab_ == 2) {
        // Артисты
        if (results_.artists.empty()) {
            r->drawText("Нет артистов", Theme::SCREEN_W / 2, contentY + 50,
                         Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
        }
        for (int i = scrollOffset_; i < static_cast<int>(results_.artists.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
            int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
            bool selected = (i == selectedIndex_);
            r->drawListItem(0, itemY, Theme::SCREEN_W, results_.artists[i].name,
                           "Артист", nullptr, selected, false);
        }
    }

    r->setClipRect(nullptr);

    // Статус скачивания
    if (!statusMessage_.empty()) {
        r->drawTextTruncated(statusMessage_, Theme::PADDING, Theme::SCREEN_H - 60,
                              Theme::SCREEN_W - Theme::PADDING * 2,
                              Theme::FONT_SMALL, Theme::ACCENT);
    }

    // Подсказки
    std::vector<std::pair<std::string, std::string>> hints;
    if (activeTab_ == 0) {
        hints = {
            {"A", "Выбрать"}, {"ZR", "Скачать"}, {"X", "Новый поиск"}, {"Y", "Лайк"},
            {"L/R", "Вкладка"}, {"B", "Назад"}
        };
    } else {
        hints = {
            {"A", "Выбрать"}, {"X", "Новый поиск"}, {"Y", "Лайк"},
            {"L/R", "Вкладка"}, {"B", "Назад"}
        };
    }
    r->drawButtonHints(hints);
}

void SearchScreen::onEnter() {
    if (query_.empty()) {
        performSearch();
    }
}

void SearchScreen::onExit() {}

void SearchScreen::performSearch() {
    std::string input;
    if (Keyboard::getText("Поиск в Яндекс Музыке", input, query_)) {
        if (!input.empty()) {
            query_ = input;
            loading_ = true;
            results_ = app_->getAPI()->search(query_);
            hasResults_ = !results_.tracks.empty() || !results_.albums.empty() || !results_.artists.empty();
            loading_ = false;
            activeTab_ = 0;
            selectedIndex_ = 0;
            scrollOffset_ = 0;
        }
    }
}
