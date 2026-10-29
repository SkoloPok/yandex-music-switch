// Реализация экрана авторизации

#include "login_screen.h"
#include "../app.h"
#include "theme.h"
#include "keyboard.h"
#include "../api/auth.h"

LoginScreen::LoginScreen(App* app)
    : Screen(app) {}

void LoginScreen::handleInput(uint64_t kDown, uint64_t /*kHeld*/) {
    if (state_ == State::Validating || state_ == State::Success) return;

    // [A] — ввести токен через системную клавиатуру
    if (kDown & HidNpadButton_A) {
        std::string input;
        if (Keyboard::getText("Введите OAuth-токен Яндекс Музыки", input)) {
            if (!input.empty()) {
                token_ = input;
                tokenEntered_ = true;
                validateToken(token_);
            }
        }
    }

    // [+] — выход
    if (kDown & HidNpadButton_Plus) {
        app_->quit();
    }
}

void LoginScreen::update() {
    if (state_ == State::Success) {
        successTimer_++;
        if (successTimer_ >= 60) { // ~1 секунда при 60fps
            app_->navigateTo(AppScreen::HOME);
        }
    }
}

void LoginScreen::render() {
    auto* r = app_->getRenderer();

    // Заголовок
    r->drawText("Яндекс Музыка", Theme::SCREEN_W / 2, 140,
                Theme::FONT_TITLE + 10, Theme::ACCENT, TextAlign::CENTER);

    r->drawText("Nintendo Switch Edition", Theme::SCREEN_W / 2, 195,
                Theme::FONT_NORMAL, Theme::TEXT_SECONDARY, TextAlign::CENTER);

    // Инструкция
    r->drawText("Для авторизации вам понадобится OAuth-токен.", Theme::SCREEN_W / 2, 270,
                Theme::FONT_SMALL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
    r->drawText("Получите его на oauth.yandex.ru", Theme::SCREEN_W / 2, 298,
                Theme::FONT_SMALL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
    r->drawText("Client ID: 23cabbbdc6cd418abb4b39c32c41195d", Theme::SCREEN_W / 2, 326,
                Theme::FONT_TINY, Theme::TEXT_HINT, TextAlign::CENTER);

    // Поле ввода токена
    int fieldW = 500, fieldH = 50;
    int fieldX = (Theme::SCREEN_W - fieldW) / 2, fieldY = 390;

    r->drawRect(fieldX, fieldY, fieldW, fieldH, Theme::BG_SECONDARY);
    r->drawRectOutline(fieldX, fieldY, fieldW, fieldH, Theme::TEXT_HINT);

    if (tokenEntered_ && !token_.empty()) {
        // Маскируем токен
        std::string masked;
        if (token_.size() > 4) {
            masked = token_.substr(0, 4) + std::string(std::min<size_t>(token_.size() - 4, 20), '*');
        } else {
            masked = std::string(token_.size(), '*');
        }
        r->drawTextTruncated(masked, fieldX + 16, fieldY + 14, fieldW - 32,
                             Theme::FONT_SMALL, Theme::TEXT_PRIMARY);
    } else {
        r->drawText("Нажмите A чтобы ввести токен", fieldX + fieldW / 2, fieldY + 14,
                     Theme::FONT_SMALL, Theme::TEXT_HINT, TextAlign::CENTER);
    }

    // Статус
    int msgY = 480;
    switch (state_) {
        case State::Validating:
            r->drawText("Проверка токена...", Theme::SCREEN_W / 2, msgY,
                        Theme::FONT_NORMAL, Theme::TEXT_SECONDARY, TextAlign::CENTER);
            break;
        case State::Success:
            r->drawText("Добро пожаловать, " + userName_ + "!",
                        Theme::SCREEN_W / 2, msgY,
                        Theme::FONT_NORMAL, Theme::ACCENT, TextAlign::CENTER);
            break;
        case State::Error:
            r->drawText(errorMessage_, Theme::SCREEN_W / 2, msgY,
                        Theme::FONT_NORMAL, Theme::ACCENT_RED, TextAlign::CENTER);
            break;
        case State::Idle:
            break;
    }

    // Подсказки кнопок
    std::vector<std::pair<std::string, std::string>> hints;
    if (state_ != State::Validating && state_ != State::Success) {
        hints.push_back({"A", "Ввести токен"});
    }
    hints.push_back({"+", "Выход"});
    r->drawButtonHints(hints);
}

void LoginScreen::onEnter() {
    state_ = State::Idle;
    token_.clear();
    userName_.clear();
    errorMessage_.clear();
    successTimer_ = 0;
    tokenEntered_ = false;

    // Автозагрузка сохранённого токена
    if (AuthManager::hasToken()) {
        std::string savedToken = AuthManager::loadToken();
        if (!savedToken.empty()) {
            token_ = savedToken;
            tokenEntered_ = true;
            validateToken(token_);
        }
    }
}

void LoginScreen::onExit() {}

void LoginScreen::validateToken(const std::string& token) {
    state_ = State::Validating;
    errorMessage_.clear();

    app_->getAPI()->setToken(token);

    UserInfo info;
    bool ok = app_->getAPI()->getAccountStatus(info);

    if (ok && !info.displayName.empty()) {
        AuthManager::saveToken(token);
        userName_ = info.displayName;
        state_ = State::Success;
        successTimer_ = 0;
    } else {
        state_ = State::Error;
        errorMessage_ = "Неверный токен. Проверьте и попробуйте снова.";
    }
}
