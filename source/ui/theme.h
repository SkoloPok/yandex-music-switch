/// @file theme.h
/// @brief Цветовая схема и константы стилей для приложения Yandex Music.
/// 
/// Тёмная тема, вдохновлённая дизайном Yandex Music:
/// тёмный фон, жёлтые акценты, белый и серый текст.
/// Все размеры рассчитаны для экрана Nintendo Switch 1280×720.

#pragma once
#include <SDL2/SDL.h>

namespace Theme {

    // ─── Цвета фона ───────────────────────────────────────────────
    constexpr SDL_Color BG_PRIMARY   = {18,  18,  18,  255};   // Почти чёрный — основной фон
    constexpr SDL_Color BG_SECONDARY = {30,  30,  30,  255};   // Чуть светлее — второстепенный фон
    constexpr SDL_Color BG_CARD      = {40,  40,  40,  255};   // Фон карточки
    constexpr SDL_Color BG_SELECTED  = {55,  55,  55,  255};   // Выделенный элемент
    constexpr SDL_Color BG_PLAYER    = {24,  24,  24,  255};   // Фон панели плеера

    // ─── Цвета текста ─────────────────────────────────────────────
    constexpr SDL_Color TEXT_PRIMARY   = {255, 255, 255, 255};  // Белый — основной текст
    constexpr SDL_Color TEXT_SECONDARY = {180, 180, 180, 255};  // Серый — второстепенный текст
    constexpr SDL_Color TEXT_HINT      = {120, 120, 120, 255};  // Тёмно-серый — подсказки

    // ─── Акцентные цвета (фирменный жёлтый Яндекса) ──────────────
    constexpr SDL_Color ACCENT       = {255, 219, 77,  255};   // Жёлтый Яндекса
    constexpr SDL_Color ACCENT_HOVER = {255, 230, 120, 255};   // Светло-жёлтый (наведение)
    constexpr SDL_Color ACCENT_RED   = {255, 51,  51,  255};   // Красный (лайк / сердечко)

    // ─── Прогресс-бар ─────────────────────────────────────────────
    constexpr SDL_Color PROGRESS_BG   = {60,  60,  60,  255};  // Фон прогресс-бара
    constexpr SDL_Color PROGRESS_FILL = {255, 219, 77,  255};  // Заполнение (жёлтый)

    // ─── Размеры экрана и элементов (в пикселях) ──────────────────
    constexpr int SCREEN_W        = 1280;
    constexpr int SCREEN_H        = 720;
    constexpr int PLAYER_BAR_H    = 80;     // Высота панели плеера (низ экрана)
    constexpr int TAB_BAR_H       = 50;     // Высота панели вкладок (верх экрана)
    constexpr int PADDING         = 16;     // Стандартный отступ
    constexpr int ITEM_HEIGHT     = 72;     // Высота строки списка
    constexpr int COVER_SIZE      = 56;     // Размер обложки в списке
    constexpr int LARGE_COVER_SIZE = 200;   // Большая обложка (экран плеера)

    // ─── Размеры шрифтов (pt) ─────────────────────────────────────
    constexpr int FONT_TITLE  = 28;
    constexpr int FONT_NORMAL = 22;
    constexpr int FONT_SMALL  = 18;
    constexpr int FONT_TINY   = 14;

    // ─── Прочие константы ─────────────────────────────────────────
    constexpr int COVER_CACHE_MAX = 50;     // Максимум текстур обложек в кэше

} // namespace Theme
