// Реализация панели плеера внизу экрана

#include "player_bar.h"
#include "../app.h"
#include "theme.h"

PlayerBar::PlayerBar(App* app) : app_(app) {}

bool PlayerBar::isVisible() const {
    auto* player = app_->getPlayer();
    return player->getCurrentTrack() != nullptr;
}

void PlayerBar::update() {
    // Обновление анимаций (если нужно)
}

void PlayerBar::render() {
    if (!isVisible()) return;

    auto* r = app_->getRenderer();
    auto* player = app_->getPlayer();
    const Track* track = player->getCurrentTrack();
    if (!track) return;

    int barY = Theme::SCREEN_H - Theme::PLAYER_BAR_H;

    // Фон панели
    r->drawRect(0, barY, Theme::SCREEN_W, Theme::PLAYER_BAR_H, Theme::BG_PLAYER);

    // Разделительная линия сверху
    r->drawRect(0, barY, Theme::SCREEN_W, 1, Theme::BG_CARD);

    // Прогресс-бар (тонкий, 3px)
    float progress = player->getProgress();
    r->drawProgressBar(0, barY + 1, Theme::SCREEN_W, 3, progress);

    int contentY = barY + 6;

    // Обложка (плейсхолдер)
    int coverX = Theme::PADDING;
    int coverY = contentY + 4;
    int coverSz = Theme::COVER_SIZE;
    r->drawRect(coverX, coverY, coverSz, coverSz, Theme::BG_CARD);
    r->drawText("♪", coverX + coverSz / 2, coverY + coverSz / 4,
                 Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);

    // Информация о треке
    int textX = coverX + coverSz + Theme::PADDING;
    int maxTextW = Theme::SCREEN_W - textX - 250;

    r->drawTextTruncated(track->title, textX, contentY + 8, maxTextW,
                          Theme::FONT_NORMAL, Theme::TEXT_PRIMARY);
    r->drawTextTruncated(track->getArtistName(), textX, contentY + 34, maxTextW,
                          Theme::FONT_SMALL, Theme::TEXT_SECONDARY);

    // Состояние воспроизведения
    PlayerState state = player->getState();
    std::string stateIcon;
    switch (state) {
        case PlayerState::PLAYING: stateIcon = "▶"; break;
        case PlayerState::PAUSED:  stateIcon = "⏸"; break;
        case PlayerState::LOADING: stateIcon = "⏳"; break;
        default:                   stateIcon = "⏹"; break;
    }

    // Кнопки управления
    int controlsX = Theme::SCREEN_W - 230;
    r->drawText("⏮", controlsX, contentY + 18, Theme::FONT_NORMAL, Theme::TEXT_SECONDARY);
    r->drawText(stateIcon, controlsX + 50, contentY + 18, Theme::FONT_NORMAL, Theme::ACCENT);
    r->drawText("⏭", controlsX + 100, contentY + 18, Theme::FONT_NORMAL, Theme::TEXT_SECONDARY);

    // Время
    int posMs = player->getPositionMs();
    int durMs = player->getDurationMs();

    int posSec = posMs / 1000;
    int posMin = posSec / 60;
    posSec %= 60;

    int durSec = durMs / 1000;
    int durMin = durSec / 60;
    durSec %= 60;

    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%d:%02d / %d:%02d", posMin, posSec, durMin, durSec);
    r->drawText(timeStr, Theme::SCREEN_W - Theme::PADDING, contentY + 48,
                 Theme::FONT_TINY, Theme::TEXT_HINT, TextAlign::RIGHT);
}
