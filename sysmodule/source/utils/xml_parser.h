#pragma once

// Минимальный XML-парсер для разбора ответа Yandex Music download-info
//
// Формат XML-ответа:
// <download-info>
//   <host>s150vla.storage.yandex.net</host>
//   <path>/rmusic/U2FsdGVkX1.../1234567.mp3</path>
//   <ts>000583b5e7e...</ts>
//   <region>-1</region>
//   <s>abcdef1234567890</s>
// </download-info>
//
// Парсер использует простой поиск по строкам — не требует внешних XML-библиотек.

#include <string>

/// Структура с данными для скачивания трека
struct DownloadInfo {
    std::string host;    // Хост хранилища (например, s150vla.storage.yandex.net)
    std::string path;    // Путь к файлу на хранилище
    std::string ts;      // Временная метка (timestamp)
    std::string s;       // Подпись (sign seed)
    std::string region;  // Регион (-1 по умолчанию)
};

/// Парсер XML-ответа download-info
class XmlParser {
public:
    /// Разобрать XML-строку download-info и вернуть структуру DownloadInfo
    static DownloadInfo parseDownloadInfo(const std::string& xml);

private:
    /// Извлечь содержимое тега <tagName>...</tagName> из XML-строки
    static std::string extractTag(const std::string& xml, const std::string& tagName);
};
