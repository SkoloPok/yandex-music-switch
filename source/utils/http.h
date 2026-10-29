#pragma once

// HTTP-клиент — обёртка над libcurl для Nintendo Switch
// Перед использованием необходимо вызвать socketInitializeDefault() (делается в main.cpp)
//
// Возможности:
// - GET-запросы с произвольными заголовками (в т.ч. Authorization: OAuth <token>)
// - POST-запросы с данными формы
// - Ответ сохраняется в std::string
// - Скачивание бинарных данных в буфер памяти (для аудиофайлов)
// - Настройки таймаута
// - Потокобезопасность (каждый запрос создаёт свой CURL-хэндл)
// - CA-сертификат загружается из "romfs:/cacert.pem"

#include <string>
#include <vector>
#include <cstdint>
#include <utility>

#include <curl/curl.h>

/// Структура ответа HTTP-запроса
struct HttpResponse {
    long statusCode;              // HTTP-код ответа (200, 404 и т.д.)
    std::string body;             // Тело ответа (текст)
    std::vector<uint8_t> binaryData; // Бинарные данные (для скачивания аудио)
    bool success;                 // Флаг успешности запроса (без сетевых ошибок)
    std::string error;            // Описание ошибки (если success == false)
};

/// HTTP-клиент для работы с Yandex Music API
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    /// Установить OAuth-токен для авторизации
    void setAuthToken(const std::string& token);

    /// GET-запрос с опциональными дополнительными заголовками
    HttpResponse get(const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers = {});

    /// POST-запрос с данными и опциональными дополнительными заголовками
    HttpResponse post(const std::string& url,
                      const std::string& postData,
                      const std::vector<std::pair<std::string, std::string>>& headers = {});

    /// Скачать бинарные данные (аудиофайл) по URL
    HttpResponse downloadBinary(const std::string& url);

private:
    std::string authToken_;

    /// Колбэк записи текстовых данных (для body)
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /// Колбэк записи бинарных данных (для binaryData)
    static size_t binaryWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /// Настройка общих параметров CURL-хэндла
    void setupCommonOptions(CURL* curl, const std::string& url,
                            const std::vector<std::pair<std::string, std::string>>& headers,
                            struct curl_slist** headerList);
};
