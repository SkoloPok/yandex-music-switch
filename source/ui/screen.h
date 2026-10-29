#pragma once
// Базовый класс экрана приложения
// Все экраны (логин, домашний, поиск, плейлист) наследуют этот интерфейс

#include <cstdint>

#ifdef __SWITCH__
#include <switch.h>
#else
// Заглушки кнопок контроллера Switch для сборки/тестирования вне Switch
enum HidNpadButton : uint64_t {
    HidNpadButton_A          = 1ULL << 0,
    HidNpadButton_B          = 1ULL << 1,
    HidNpadButton_X          = 1ULL << 2,
    HidNpadButton_Y          = 1ULL << 3,
    HidNpadButton_StickL     = 1ULL << 4,
    HidNpadButton_StickR     = 1ULL << 5,
    HidNpadButton_L          = 1ULL << 6,
    HidNpadButton_R          = 1ULL << 7,
    HidNpadButton_ZL         = 1ULL << 8,
    HidNpadButton_ZR         = 1ULL << 9,
    HidNpadButton_Plus       = 1ULL << 10,
    HidNpadButton_Minus      = 1ULL << 11,
    HidNpadButton_Left       = 1ULL << 12,
    HidNpadButton_Up         = 1ULL << 13,
    HidNpadButton_Right      = 1ULL << 14,
    HidNpadButton_Down       = 1ULL << 15,
    HidNpadButton_StickLLeft = 1ULL << 16,
    HidNpadButton_StickLUp   = 1ULL << 17,
    HidNpadButton_StickLRight= 1ULL << 18,
    HidNpadButton_StickLDown = 1ULL << 19,
    HidNpadButton_StickRLeft = 1ULL << 20,
    HidNpadButton_StickRUp   = 1ULL << 21,
    HidNpadButton_StickRRight= 1ULL << 22,
    HidNpadButton_StickRDown = 1ULL << 23,
};
#endif

class App; // forward declaration

class Screen {
public:
    explicit Screen(App* app) : app_(app) {}
    virtual ~Screen() = default;

    // Вызывается при переходе на этот экран
    virtual void onEnter() {}
    // Вызывается при уходе с этого экрана
    virtual void onExit() {}

    // Обработка ввода контроллера
    virtual void handleInput(uint64_t kDown, uint64_t kHeld) = 0;
    // Обновление логики
    virtual void update() = 0;
    // Отрисовка экрана
    virtual void render() = 0;

protected:
    App* app_;
};
