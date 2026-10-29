#pragma once
// Управление OAuth-токеном — чтение/запись на SD-карту

#include <string>

class AuthManager {
public:
    // Загрузить токен из файла на SD-карте
    static std::string loadToken();
    // Сохранить токен в файл на SD-карте
    static bool saveToken(const std::string& token);
    // Удалить файл токена
    static bool deleteToken();
    // Проверить, существует ли файл токена
    static bool hasToken();

private:
    static constexpr const char* TOKEN_DIR  = "sdmc:/switch/yandex-music";
    static constexpr const char* TOKEN_PATH = "sdmc:/switch/yandex-music/token.txt";
};
