#pragma once
// Экран авторизации — ввод OAuth-токена Яндекс Музыки

#include "screen.h"
#include "../api/types.h"
#include "../api/yandex_api.h"
#include <string>

class App;

class LoginScreen : public Screen {
public:
    explicit LoginScreen(App* app);

    void handleInput(uint64_t kDown, uint64_t kHeld) override;
    void update() override;
    void render() override;
    void onEnter() override;
    void onExit() override;

private:
    enum class State {
        Idle,
        Validating,
        Success,
        Error
    };

    void validateToken(const std::string& token);

    State state_ = State::Idle;
    std::string token_;
    std::string userName_;
    std::string errorMessage_;
    int successTimer_ = 0;
    bool tokenEntered_ = false;
};
