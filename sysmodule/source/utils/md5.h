#pragma once

// Автономная реализация хеш-функции MD5 (RFC 1321)
// Не требует внешних зависимостей.
//
// Используется для формирования URL скачивания треков Yandex Music:
//   sign = MD5("XGRlBW9FXlekgbPrRHuSiA" + path.substr(1) + s)

#include <string>

namespace MD5 {
    /// Вычислить MD5-хеш строки и вернуть результат в виде 32-символьной hex-строки (lowercase)
    std::string hash(const std::string& input);
}
