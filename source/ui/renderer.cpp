// Реализация рендерера — обёртка над SDL2 для рисования UI
// Загрузка системного шрифта Switch, кэш обложек, отрисовка элементов списков

#include "renderer.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
#endif

Renderer::Renderer(SDL_Renderer* renderer)
    : renderer_(renderer) {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
#ifdef __SWITCH__
    // Загружаем системный шрифт Nintendo Switch
    PlFontData fontData;
    Result rc = plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    if (R_SUCCEEDED(rc)) {
        systemFontData_ = fontData.address;
        systemFontSize_ = fontData.size;
    } else {
        return false;
    }
#else
    // Для отладки вне Switch
    systemFontData_ = nullptr;
    systemFontSize_ = 0;
#endif
    return true;
}

void Renderer::shutdown() {
    // Освобождаем шрифты
    for (auto& [size, font] : fonts_) {
        if (font) TTF_CloseFont(font);
    }
    fonts_.clear();

    // Освобождаем кэш обложек
    for (auto& [url, tex] : coverCache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    coverCache_.clear();
    coverLruOrder_.clear();

    // systemFontData_ управляется pl-сервисом — не освобождаем
}

TTF_Font* Renderer::getFont(int size) {
    auto it = fonts_.find(size);
    if (it != fonts_.end()) return it->second;

    TTF_Font* font = nullptr;
    if (systemFontData_ && systemFontSize_ > 0) {
        SDL_RWops* rw = SDL_RWFromMem(systemFontData_, static_cast<int>(systemFontSize_));
        if (rw) {
            font = TTF_OpenFontRW(rw, 1, size);
        }
    }
#ifndef __SWITCH__
    if (!font) {
        font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", size);
    }
    if (!font) {
        font = TTF_OpenFont("romfs:/fonts/font.ttf", size);
    }
#endif

    fonts_[size] = font;
    return font;
}

// ============================================================================
// Базовые операции
// ============================================================================

void Renderer::clear(SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void Renderer::present() {
    SDL_RenderPresent(renderer_);
}

void Renderer::setClipRect(const SDL_Rect* rect) {
    SDL_RenderSetClipRect(renderer_, rect);
}

// ============================================================================
// Прямоугольники
// ============================================================================

void Renderer::drawRect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void Renderer::drawRectOutline(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}

void Renderer::drawRoundedRect(int x, int y, int w, int h, int radius, SDL_Color color) {
    // Простая реализация — рисуем обычный прямоугольник
    // Для скруглений нужен SDL2_gfx, но простой rect достаточно для MVP
    drawRect(x, y, w, h, color);
}

// ============================================================================
// Текст
// ============================================================================

void Renderer::drawText(const std::string& text, int x, int y, int fontSize,
                        SDL_Color color, TextAlign align) {
    if (text.empty()) return;

    TTF_Font* font = getFont(fontSize);
    if (!font) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst;
    dst.w = surface->w;
    dst.h = surface->h;

    switch (align) {
        case TextAlign::LEFT:
            dst.x = x;
            break;
        case TextAlign::CENTER:
            dst.x = x - dst.w / 2;
            break;
        case TextAlign::RIGHT:
            dst.x = x - dst.w;
            break;
    }
    dst.y = y;

    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void Renderer::drawTextTruncated(const std::string& text, int x, int y, int maxWidth,
                                  int fontSize, SDL_Color color) {
    if (text.empty()) return;

    int textW = getTextWidth(text, fontSize);
    if (textW <= maxWidth) {
        drawText(text, x, y, fontSize, color, TextAlign::LEFT);
        return;
    }

    // Обрезаем текст и добавляем "..."
    std::string truncated = text;
    while (!truncated.empty()) {
        // Убираем последний UTF-8 символ
        while (!truncated.empty() && (truncated.back() & 0xC0) == 0x80) {
            truncated.pop_back();
        }
        if (!truncated.empty()) {
            truncated.pop_back();
        }

        std::string withEllipsis = truncated + "...";
        if (getTextWidth(withEllipsis, fontSize) <= maxWidth) {
            drawText(withEllipsis, x, y, fontSize, color, TextAlign::LEFT);
            return;
        }
    }

    drawText("...", x, y, fontSize, color, TextAlign::LEFT);
}

int Renderer::getTextWidth(const std::string& text, int fontSize) {
    if (text.empty()) return 0;

    TTF_Font* font = getFont(fontSize);
    if (!font) return 0;

    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

// ============================================================================
// Текстуры
// ============================================================================

SDL_Texture* Renderer::loadTextureFromMemory(const std::vector<uint8_t>& data) {
    if (data.empty()) return nullptr;

    SDL_RWops* rw = SDL_RWFromConstMem(data.data(), static_cast<int>(data.size()));
    if (!rw) return nullptr;

    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);

    return texture;
}

void Renderer::drawTexture(SDL_Texture* texture, int x, int y, int w, int h) {
    if (!texture) return;
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::drawTexture(SDL_Texture* texture, const SDL_Rect& dst) {
    if (!texture) return;
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::destroyTexture(SDL_Texture* texture) {
    if (texture) SDL_DestroyTexture(texture);
}

// ============================================================================
// Кэш обложек (LRU)
// ============================================================================

SDL_Texture* Renderer::getCoverTexture(const std::string& url) {
    auto it = coverCache_.find(url);
    if (it != coverCache_.end()) {
        // Перемещаем в конец LRU списка (самый свежий)
        coverLruOrder_.remove(url);
        coverLruOrder_.push_back(url);
        return it->second;
    }
    return nullptr;
}

void Renderer::cacheCoverTexture(const std::string& url, SDL_Texture* texture) {
    // Если уже есть — удаляем старый
    auto it = coverCache_.find(url);
    if (it != coverCache_.end()) {
        SDL_DestroyTexture(it->second);
        coverCache_.erase(it);
        coverLruOrder_.remove(url);
    }

    // Вытесняем самый старый, если кэш полон
    while (static_cast<int>(coverCache_.size()) >= Theme::COVER_CACHE_MAX && !coverLruOrder_.empty()) {
        const std::string& oldest = coverLruOrder_.front();
        auto oldIt = coverCache_.find(oldest);
        if (oldIt != coverCache_.end()) {
            SDL_DestroyTexture(oldIt->second);
            coverCache_.erase(oldIt);
        }
        coverLruOrder_.pop_front();
    }

    coverCache_[url] = texture;
    coverLruOrder_.push_back(url);
}

// ============================================================================
// Составные элементы
// ============================================================================

void Renderer::drawProgressBar(int x, int y, int w, int h, float progress,
                                SDL_Color bg, SDL_Color fill) {
    // Фон
    drawRect(x, y, w, h, bg);
    // Заполнение
    int fillW = static_cast<int>(w * progress);
    if (fillW > 0) {
        drawRect(x, y, fillW, h, fill);
    }
}

void Renderer::drawListItem(int x, int y, int w, const std::string& title,
                             const std::string& subtitle, SDL_Texture* cover,
                             bool selected, bool playing) {
    // Фон выделенного элемента
    if (selected) {
        drawRect(x, y, w, Theme::ITEM_HEIGHT, Theme::BG_SELECTED);
    }

    int padding = Theme::PADDING;
    int coverX = x + padding;
    int coverY = y + (Theme::ITEM_HEIGHT - Theme::COVER_SIZE) / 2;

    // Обложка
    if (cover) {
        drawTexture(cover, coverX, coverY, Theme::COVER_SIZE, Theme::COVER_SIZE);
    } else {
        // Плейсхолдер обложки
        drawRect(coverX, coverY, Theme::COVER_SIZE, Theme::COVER_SIZE, Theme::BG_CARD);
        drawText("♪", coverX + Theme::COVER_SIZE / 2, coverY + Theme::COVER_SIZE / 4,
                 Theme::FONT_NORMAL, Theme::TEXT_HINT, TextAlign::CENTER);
    }

    int textX = coverX + Theme::COVER_SIZE + padding;
    int maxTextW = w - textX - padding + x;

    // Название трека
    SDL_Color titleColor = playing ? Theme::ACCENT : Theme::TEXT_PRIMARY;
    drawTextTruncated(title, textX, y + 12, maxTextW, Theme::FONT_NORMAL, titleColor);

    // Подзаголовок (артист)
    drawTextTruncated(subtitle, textX, y + 40, maxTextW, Theme::FONT_SMALL, Theme::TEXT_SECONDARY);
}

void Renderer::drawTabBar(const std::vector<std::string>& tabs, int activeTab, int y) {
    // Фон панели вкладок
    drawRect(0, y, Theme::SCREEN_W, Theme::TAB_BAR_H, Theme::BG_SECONDARY);

    int tabWidth = Theme::SCREEN_W / static_cast<int>(tabs.size());

    for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
        int tabX = i * tabWidth;

        // Подчёркивание активной вкладки
        if (i == activeTab) {
            drawRect(tabX, y + Theme::TAB_BAR_H - 3, tabWidth, 3, Theme::ACCENT);
        }

        SDL_Color color = (i == activeTab) ? Theme::ACCENT : Theme::TEXT_SECONDARY;
        drawText(tabs[i], tabX + tabWidth / 2, y + 14, Theme::FONT_SMALL, color, TextAlign::CENTER);
    }
}

void Renderer::drawButtonHints(const std::vector<std::pair<std::string, std::string>>& hints) {
    int y = Theme::SCREEN_H - 30;
    int x = Theme::PADDING;

    for (const auto& [button, action] : hints) {
        // Кнопка
        drawText(button, x, y, Theme::FONT_TINY, Theme::ACCENT, TextAlign::LEFT);
        x += getTextWidth(button, Theme::FONT_TINY) + 6;
        // Действие
        drawText(action, x, y, Theme::FONT_TINY, Theme::TEXT_HINT, TextAlign::LEFT);
        x += getTextWidth(action, Theme::FONT_TINY) + 20;
    }
}
