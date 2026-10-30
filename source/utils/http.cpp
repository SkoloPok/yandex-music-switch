// Реализация HTTP-клиента на основе libcurl для Nintendo Switch

#include "http.h"

#include <cstring>

// Таймаут соединения (секунды)
static constexpr long CONNECT_TIMEOUT = 10L;
// Таймаут выполнения запроса (секунды)
static constexpr long REQUEST_TIMEOUT = 30L;
// Таймаут для скачивания бинарных данных (секунды)
static constexpr long DOWNLOAD_TIMEOUT = 120L;
// Путь к CA-сертификату в romfs
static const char* CA_CERT_PATH = "romfs:/cacert.pem";

// ============================================================================
// Конструктор / Деструктор
// ============================================================================

HttpClient::HttpClient() {
    // Глобальная инициализация curl (безопасно вызывать повторно,
    // но лучше один раз при старте приложения)
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

// ============================================================================
// Публичные методы
// ============================================================================

void HttpClient::setAuthToken(const std::string& token) {
    authToken_ = token;
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::vector<std::pair<std::string, std::string>>& headers) {
    HttpResponse response{};
    response.statusCode = 0;
    response.success = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL handle";
        return response;
    }

    struct curl_slist* headerList = nullptr;
    setupCommonOptions(curl, url, headers, &headerList);

    // Настройка колбэка для записи текстовых данных
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    // Таймаут запроса
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT);

    // Выполнение запроса
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = std::string("CURL error: ") + curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.success = true;
    }

    // Очистка
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse HttpClient::post(const std::string& url,
                              const std::string& postData,
                              const std::vector<std::pair<std::string, std::string>>& headers) {
    HttpResponse response{};
    response.statusCode = 0;
    response.success = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL handle";
        return response;
    }

    struct curl_slist* headerList = nullptr;
    setupCommonOptions(curl, url, headers, &headerList);

    // Настройка POST-запроса
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(postData.size()));

    // Настройка колбэка для записи текстовых данных
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    // Таймаут запроса
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT);

    // Выполнение запроса
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = std::string("CURL error: ") + curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.success = true;
    }

    // Очистка
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse HttpClient::downloadBinary(const std::string& url) {
    HttpResponse response{};
    response.statusCode = 0;
    response.success = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL handle";
        return response;
    }

    // Для бинарного скачивания используем минимальный набор заголовков
    struct curl_slist* headerList = nullptr;
    std::vector<std::pair<std::string, std::string>> emptyHeaders;
    setupCommonOptions(curl, url, emptyHeaders, &headerList);

    // Настройка колбэка для записи бинарных данных
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, binaryWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.binaryData);

    // Увеличенный таймаут для скачивания аудио
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DOWNLOAD_TIMEOUT);

    // Выполнение запроса
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = std::string("CURL error: ") + curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.success = true;
    }

    // Очистка
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse HttpClient::downloadToFile(const std::string& url, const std::string& filePath) {
    HttpResponse response{};
    response.statusCode = 0;
    response.success = false;

    FILE* fp = fopen(filePath.c_str(), "wb");
    if (!fp) {
        response.error = "Failed to open file for writing: " + filePath;
        return response;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        response.error = "Failed to initialize CURL handle";
        return response;
    }

    struct curl_slist* headerList = nullptr;
    std::vector<std::pair<std::string, std::string>> emptyHeaders;
    setupCommonOptions(curl, url, emptyHeaders, &headerList);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DOWNLOAD_TIMEOUT);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        response.error = std::string("CURL error: ") + curl_easy_strerror(res);
        remove(filePath.c_str());
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
        if (!response.success) {
            remove(filePath.c_str());
        }
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return response;
}

// ============================================================================
// Приватные методы
// ============================================================================

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* body = static_cast<std::string*>(userp);
    body->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t HttpClient::binaryWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* buffer = static_cast<std::vector<uint8_t>*>(userp);
    auto* data = static_cast<uint8_t*>(contents);
    buffer->insert(buffer->end(), data, data + totalSize);
    return totalSize;
}

size_t HttpClient::fileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* fp = static_cast<FILE*>(userp);
    return fwrite(contents, size, nmemb, fp);
}

void HttpClient::setupCommonOptions(CURL* curl, const std::string& url,
                                    const std::vector<std::pair<std::string, std::string>>& headers,
                                    struct curl_slist** headerList) {
    // URL запроса
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Следовать редиректам (Yandex может перенаправлять)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    // Таймаут соединения
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);

    // SSL: использовать CA-сертификат из romfs на Switch
#ifdef __SWITCH__
    curl_easy_setopt(curl, CURLOPT_CAINFO, CA_CERT_PATH);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#else
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    // User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "YandexMusic-NX/1.0");

    // Добавить заголовок авторизации, если токен установлен
    if (!authToken_.empty()) {
        std::string authHeader = "Authorization: OAuth " + authToken_;
        *headerList = curl_slist_append(*headerList, authHeader.c_str());
    }

    // Добавить пользовательские заголовки
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        *headerList = curl_slist_append(*headerList, header.c_str());
    }

    // Установить список заголовков (может быть nullptr, если заголовков нет — curl это допускает)
    if (*headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headerList);
    }
}
