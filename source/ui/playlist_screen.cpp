// Реализация экрана плейлиста

#include "playlist_screen.h"
#include "../app.h"
#include "theme.h"

PlaylistScreen::PlaylistScreen(App* app)
    : Screen(app) {}

void PlaylistScreen::setPlaylist(const Playlist& playlist) {
    playlist_ = playlist;
    tracksLoaded_ = !playlist_.tracks.empty();
    selectedIndex_ = 0;
    scrollOffset_ = 0;
}

void PlaylistScreen::handleInput(uint64_t kDown, uint64_t /*kHeld*/) {
    if (loading_ || isDownloading_) return;

    // B — назад
    if (kDown & HidNpadButton_B) {
        app_->goBack();
        return;
    }

    int listSize = static_cast<int>(playlist_.tracks.size());

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

    // A — воспроизвести
    if (kDown & HidNpadButton_A) {
        if (selectedIndex_ < listSize) {
            app_->playPlaylist(playlist_.tracks, selectedIndex_);
        }
    }

    // X — скачать выбранный трек
    if (kDown & HidNpadButton_X) {
        downloadSelectedTrack();
    }

    // ZR или R3 — скачать весь плейлист
    if (kDown & (HidNpadButton_ZR | HidNpadButton_StickR)) {
        downloadAllTracks();
    }

    // Y — лайк
    if (kDown & HidNpadButton_Y) {
        if (selectedIndex_ < listSize) {
            app_->getAPI()->likeTrack(playlist_.tracks[selectedIndex_].id);
        }
    }
}

void PlaylistScreen::update() {
    if (statusTimer_ > 0) {
        statusTimer_--;
        if (statusTimer_ == 0 && !isDownloading_) {
            statusMessage_.clear();
        }
    }
}

void PlaylistScreen::render() {
    auto* r = app_->getRenderer();

    // Заголовок плейлиста
    int headerH = 120;
    r->drawRect(0, 0, Theme::SCREEN_W, headerH, Theme::BG_SECONDARY);

    // Обложка плейлиста (плейсхолдер)
    int coverX = Theme::PADDING;
    int coverY = (headerH - Theme::LARGE_COVER_SIZE / 2) / 2;
    int coverSz = Theme::LARGE_COVER_SIZE / 2; // 100x100
    r->drawRect(coverX, coverY, coverSz, coverSz, Theme::BG_CARD);
    r->drawText("♪", coverX + coverSz / 2, coverY + coverSz / 4,
                 Theme::FONT_TITLE, Theme::TEXT_HINT, TextAlign::CENTER);

    // Информация о плейлисте
    int textX = coverX + coverSz + Theme::PADDING;
    r->drawTextTruncated(playlist_.title, textX, coverY + 5,
                          Theme::SCREEN_W - textX - Theme::PADDING,
                          Theme::FONT_TITLE, Theme::TEXT_PRIMARY);

    std::string info = std::to_string(playlist_.tracks.size()) + " треков";
    r->drawText(info, textX, coverY + 40, Theme::FONT_SMALL, Theme::TEXT_SECONDARY);

    // Статус скачивания или подсказка
    if (!statusMessage_.empty()) {
        r->drawTextTruncated(statusMessage_, textX, coverY + 68,
                              Theme::SCREEN_W - textX - Theme::PADDING,
                              Theme::FONT_SMALL, Theme::ACCENT);
    } else {
        r->drawText("▶ [A] Играть  •  [X] Скачать трек  •  [ZR] Скачать всё", textX, coverY + 68,
                     Theme::FONT_SMALL, Theme::ACCENT);
    }

    // Список треков
    int contentY = headerH;
    int contentH = Theme::SCREEN_H - headerH - Theme::PLAYER_BAR_H - 35;
    SDL_Rect clip = {0, contentY, Theme::SCREEN_W, contentH};
    r->setClipRect(&clip);

    if (loading_) {
        r->drawText("Загрузка...", Theme::SCREEN_W / 2, contentY + contentH / 2 - 12,
                     Theme::FONT_NORMAL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
    } else if (playlist_.tracks.empty()) {
        r->drawText("Плейлист пуст", Theme::SCREEN_W / 2, contentY + contentH / 2 - 12,
                     Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
    } else {
        const Track* currentTrack = app_->getPlayer()->getCurrentTrack();

        for (int i = scrollOffset_; i < static_cast<int>(playlist_.tracks.size()) && i < scrollOffset_ + VISIBLE_ITEMS; i++) {
            int itemY = contentY + (i - scrollOffset_) * Theme::ITEM_HEIGHT;
            bool selected = (i == selectedIndex_);
            bool playing = currentTrack && currentTrack->id == playlist_.tracks[i].id;

            // Номер трека
            std::string num = std::to_string(i + 1) + ".";
            SDL_Color numColor = playing ? Theme::ACCENT : Theme::TEXT_HINT;
            r->drawText(num, Theme::PADDING, itemY + 12, Theme::FONT_SMALL, numColor);

            // Трек
            int trackX = Theme::PADDING + 40;
            if (selected) {
                r->drawRect(trackX - 4, itemY, Theme::SCREEN_W - trackX + 4, Theme::ITEM_HEIGHT, Theme::BG_SELECTED);
            }

            bool isDownloaded = app_->getAPI()->isTrackDownloaded(playlist_.tracks[i]);
            std::string dlTag = isDownloaded ? " [SD]" : "";
            SDL_Color titleColor = playing ? Theme::ACCENT : Theme::TEXT_PRIMARY;
            r->drawTextTruncated(playlist_.tracks[i].title + dlTag, trackX, itemY + 12,
                                  Theme::SCREEN_W - trackX - 100,
                                  Theme::FONT_NORMAL, titleColor);

            r->drawTextTruncated(playlist_.tracks[i].getArtistName(), trackX, itemY + 40,
                                  Theme::SCREEN_W - trackX - 100,
                                  Theme::FONT_SMALL, Theme::TEXT_SECONDARY);

            // Длительность справа
            if (playlist_.tracks[i].durationMs > 0) {
                int sec = playlist_.tracks[i].durationMs / 1000;
                int min = sec / 60;
                sec %= 60;
                char dur[16];
                snprintf(dur, sizeof(dur), "%d:%02d", min, sec);
                r->drawText(dur, Theme::SCREEN_W - Theme::PADDING, itemY + 25,
                             Theme::FONT_SMALL, Theme::TEXT_HINT, TextAlign::RIGHT);
            }
        }
    }

    r->setClipRect(nullptr);

    // Подсказки
    std::vector<std::pair<std::string, std::string>> hints = {
        {"A", "Играть"}, {"X", "Скачать"}, {"R3", "Скачать всё"}, {"Y", "Лайк"}, {"B", "Назад"}
    };
    r->drawButtonHints(hints);
}

void PlaylistScreen::onEnter() {
    if (!tracksLoaded_) {
        loadTracks();
    }
}

void PlaylistScreen::onExit() {}

void PlaylistScreen::loadTracks() {
    if (playlist_.kind.empty()) return;

    loading_ = true;
    if (!playlist_.uid.empty()) {
        // Обычный плейлист пользователя
        Playlist full = app_->getAPI()->getPlaylist(playlist_.uid, playlist_.kind);
        playlist_.tracks = full.tracks;
        if (playlist_.title.empty()) playlist_.title = full.title;
    } else {
        // Альбом из поиска
        playlist_.tracks = app_->getAPI()->getAlbumTracks(playlist_.kind);
    }
    tracksLoaded_ = true;
    loading_ = false;
}

void PlaylistScreen::downloadSelectedTrack() {
    int listSize = static_cast<int>(playlist_.tracks.size());
    if (selectedIndex_ < 0 || selectedIndex_ >= listSize) return;

    const auto& track = playlist_.tracks[selectedIndex_];
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

void PlaylistScreen::downloadAllTracks() {
    int listSize = static_cast<int>(playlist_.tracks.size());
    if (listSize == 0) return;

    isDownloading_ = true;
    int successCount = 0;

    for (int i = 0; i < listSize; ++i) {
        const auto& track = playlist_.tracks[i];
        statusMessage_ = "Скачивание (" + std::to_string(i + 1) + "/" + std::to_string(listSize) + "): " + track.title;
        app_->render();

        if (app_->getAPI()->downloadTrackToFile(track)) {
            successCount++;
        }
    }

    statusMessage_ = "Готово! Скачано " + std::to_string(successCount) + " из " + std::to_string(listSize) + " в /music";
    statusTimer_ = 240;
    isDownloading_ = false;
}
