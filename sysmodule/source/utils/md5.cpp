// Реализация MD5 (RFC 1321) — автономная, без внешних зависимостей
//
// Алгоритм:
// 1. Дополнение сообщения до длины, кратной 512 бит (64 байта)
// 2. Разбиение на 16-словные блоки (512 бит каждый)
// 3. 4 раунда по 16 операций каждый
// 4. Результат — 128-битный хеш (4 слова по 32 бита)

#include "md5.h"

#include <cstring>
#include <cstdint>
#include <vector>

namespace {

// Сдвиги для каждого раунда (per-round shift amounts)
static constexpr uint32_t S[64] = {
    // Раунд 1
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    // Раунд 2
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    // Раунд 3
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    // Раунд 4
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

// Предвычисленные константы T[i] = floor(2^32 * |sin(i + 1)|) для i = 0..63
static constexpr uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

// Циклический сдвиг влево
inline uint32_t leftRotate(uint32_t x, uint32_t c) {
    return (x << c) | (x >> (32 - c));
}

} // anonymous namespace

namespace MD5 {

std::string hash(const std::string& input) {
    // Инициализация состояния (magic numbers из RFC 1321)
    uint32_t a0 = 0x67452301;
    uint32_t b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe;
    uint32_t d0 = 0x10325476;

    // === Шаг 1: Дополнение сообщения (padding) ===
    // Исходная длина в битах
    uint64_t originalBitLen = static_cast<uint64_t>(input.size()) * 8;

    // Копируем данные в буфер
    std::vector<uint8_t> msg(input.begin(), input.end());

    // Добавляем бит '1' (байт 0x80)
    msg.push_back(0x80);

    // Добавляем нули, пока длина не станет ≡ 56 (mod 64)
    while (msg.size() % 64 != 56) {
        msg.push_back(0x00);
    }

    // Добавляем длину исходного сообщения в битах (64 бита, little-endian)
    for (int i = 0; i < 8; ++i) {
        msg.push_back(static_cast<uint8_t>((originalBitLen >> (i * 8)) & 0xFF));
    }

    // === Шаг 2: Обработка блоков по 64 байта (512 бит) ===
    size_t numBlocks = msg.size() / 64;

    for (size_t block = 0; block < numBlocks; ++block) {
        // Разбираем блок на 16 слов по 32 бита (little-endian)
        uint32_t M[16];
        for (int j = 0; j < 16; ++j) {
            size_t offset = block * 64 + j * 4;
            M[j] = static_cast<uint32_t>(msg[offset])
                 | (static_cast<uint32_t>(msg[offset + 1]) << 8)
                 | (static_cast<uint32_t>(msg[offset + 2]) << 16)
                 | (static_cast<uint32_t>(msg[offset + 3]) << 24);
        }

        // Инициализация рабочих переменных
        uint32_t A = a0;
        uint32_t B = b0;
        uint32_t C = c0;
        uint32_t D = d0;

        // === Шаг 3: 64 операции (4 раунда по 16) ===
        for (uint32_t i = 0; i < 64; ++i) {
            uint32_t F, g;

            if (i < 16) {
                // Раунд 1: F(B, C, D) = (B & C) | (~B & D)
                F = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                // Раунд 2: G(B, C, D) = (D & B) | (~D & C)
                F = (D & B) | (~D & C);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                // Раунд 3: H(B, C, D) = B ^ C ^ D
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            } else {
                // Раунд 4: I(B, C, D) = C ^ (B | ~D)
                F = C ^ (B | ~D);
                g = (7 * i) % 16;
            }

            F = F + A + K[i] + M[g];
            A = D;
            D = C;
            C = B;
            B = B + leftRotate(F, S[i]);
        }

        // Добавляем результат блока к текущему хешу
        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }

    // === Шаг 4: Формирование результата (128 бит = 32 hex-символа) ===
    // Каждое 32-битное слово записывается в little-endian порядке байт
    uint8_t digest[16];
    for (int i = 0; i < 4; ++i) {
        digest[i]      = static_cast<uint8_t>((a0 >> (i * 8)) & 0xFF);
        digest[i + 4]  = static_cast<uint8_t>((b0 >> (i * 8)) & 0xFF);
        digest[i + 8]  = static_cast<uint8_t>((c0 >> (i * 8)) & 0xFF);
        digest[i + 12] = static_cast<uint8_t>((d0 >> (i * 8)) & 0xFF);
    }

    // Преобразуем в lowercase hex-строку
    static const char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(32);
    for (int i = 0; i < 16; ++i) {
        result.push_back(hexChars[(digest[i] >> 4) & 0x0F]);
        result.push_back(hexChars[digest[i] & 0x0F]);
    }

    return result;
}

} // namespace MD5
