/// @file renderer.h
/// @brief Утилиты рендеринга: обёртка над SDL2 для удобного рисования UI.
///
/// Класс Renderer инкапсулирует SDL_Renderer и предоставляет высокоуровневые
/// методы для отрисовки текста, прямоугольников, текстур, списков, вкладок
/// и подсказок кнопок контроллера.
///
/// Шрифты загружаются из системного шрифта Nintendo Switch (plGetSharedFont).
/// Текстуры обложек кешируются с ограничением по количеству.

#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <utility>
#include "theme.h"

/// Выравнивание текста по горизонтали.
enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

/// @class Renderer
/// @brief Высокоуровневая обёртка над SDL2 для рисования элементов интерфейса.
class Renderer {
public:
    /// @param renderer Указатель на уже созданный SDL_Renderer.
    explicit Renderer(SDL_Renderer* renderer);
    ~Renderer();

    /// Инициализация: загрузка системного шрифта Switch (plGetSharedFont).
    /// @return true при успехе, false при ошибке.
    bool init();

    /// Освобождение всех ресурсов (шрифты, кэш текстур).
    void shutdown();

    // ─── Базовые операции ──────────────────────────────────────────

    /// Очистка экрана указанным цветом.
    void clear(SDL_Color color = Theme::BG_PRIMARY);

    /// Представление отрисованного кадра на экране.
    void present();

    /// Установка прямоугольника отсечения (nullptr — сбросить).
    void setClipRect(const SDL_Rect* rect);

    // ─── Прямоугольники ────────────────────────────────────────────

    /// Заполненный прямоугольник.
    void drawRect(int x, int y, int w, int h, SDL_Color color);

    /// Контур прямоугольника (1 пиксель).
    void drawRectOutline(int x, int y, int w, int h, SDL_Color color);

    /// Скруглённый заполненный прямоугольник.
    void drawRoundedRect(int x, int y, int w, int h, int radius, SDL_Color color);

    // ─── Текст ─────────────────────────────────────────────────────

    /// Отрисовка текста.
    void drawText(const std::string& text, int x, int y, int fontSize,
                  SDL_Color color = Theme::TEXT_PRIMARY,
                  TextAlign align = TextAlign::LEFT);

    /// Отрисовка текста с обрезкой и многоточием при превышении maxWidth.
    void drawTextTruncated(const std::string& text, int x, int y, int maxWidth,
                           int fontSize, SDL_Color color = Theme::TEXT_PRIMARY);

    /// Получение ширины текста в пикселях для заданного размера шрифта.
    int getTextWidth(const std::string& text, int fontSize);

    // ─── Текстуры / изображения ────────────────────────────────────

    /// Создание текстуры из данных изображения в памяти (PNG, JPEG и т.д.).
    SDL_Texture* loadTextureFromMemory(const std::vector<uint8_t>& data);

    /// Отрисовка текстуры по координатам и размерам.
    void drawTexture(SDL_Texture* texture, int x, int y, int w, int h);

    /// Отрисовка текстуры в заданный прямоугольник.
    void drawTexture(SDL_Texture* texture, const SDL_Rect& dst);

    /// Уничтожение текстуры.
    void destroyTexture(SDL_Texture* texture);

    // ─── Кэш обложек ──────────────────────────────────────────────

    /// Получение текстуры обложки из кэша по URL. Возвращает nullptr, если не найдена.
    SDL_Texture* getCoverTexture(const std::string& url);

    /// Добавление текстуры обложки в кэш. При переполнении вытесняет самую старую.
    void cacheCoverTexture(const std::string& url, SDL_Texture* texture);

    // ─── Составные элементы ────────────────────────────────────────

    /// Прогресс-бар.
    void drawProgressBar(int x, int y, int w, int h, float progress,
                         SDL_Color bg = Theme::PROGRESS_BG,
                         SDL_Color fill = Theme::PROGRESS_FILL);

    /// Элемент списка: обложка + название + подзаголовок + выделение.
    void drawListItem(int x, int y, int w, const std::string& title,
                      const std::string& subtitle, SDL_Texture* cover,
                      bool selected, bool playing);

    /// Панель вкладок вверху экрана.
    void drawTabBar(const std::vector<std::string>& tabs, int activeTab, int y = 0);

    /// Подсказки кнопок контроллера внизу экрана (например, «A Выбрать  B Назад»).
    void drawButtonHints(const std::vector<std::pair<std::string, std::string>>& hints);

    /// Прямой доступ к SDL_Renderer (если нужен снаружи).
    SDL_Renderer* getSDLRenderer() { return renderer_; }

private:
    SDL_Renderer* renderer_;

    // Шрифты: размер (pt) → TTF_Font*
    std::unordered_map<int, TTF_Font*> fonts_;

    // Кэш обложек: URL → текстура, с порядком вставки для LRU-вытеснения
    std::unordered_map<std::string, SDL_Texture*> coverCache_;
    std::list<std::string> coverLruOrder_; // front = oldest, back = newest

    // Данные системного шрифта Switch (из plGetSharedFont)
    void* systemFontData_  = nullptr;
    size_t systemFontSize_ = 0;

    /// Получение (или создание) шрифта заданного размера.
    TTF_Font* getFont(int size);
};
