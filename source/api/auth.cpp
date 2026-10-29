#include "auth.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777)
#endif

std::string AuthManager::loadToken() {
    FILE* f = fopen(TOKEN_PATH, "r");
    if (!f) return "";

    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (n == 0) return "";

    // Убираем переносы строк и пробелы в конце
    std::string token(buf, n);
    while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' ')) {
        token.pop_back();
    }
    return token;
}

bool AuthManager::saveToken(const std::string& token) {
    // Создаём директорию, если не существует
    MKDIR("sdmc:/switch");
    MKDIR(TOKEN_DIR);

    FILE* f = fopen(TOKEN_PATH, "w");
    if (!f) return false;

    size_t written = fwrite(token.c_str(), 1, token.size(), f);
    fclose(f);

    return written == token.size();
}

bool AuthManager::deleteToken() {
    return remove(TOKEN_PATH) == 0;
}

bool AuthManager::hasToken() {
    struct stat st;
    return stat(TOKEN_PATH, &st) == 0;
}
