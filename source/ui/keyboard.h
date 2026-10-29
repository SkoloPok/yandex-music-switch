#pragma once
// Виртуальная клавиатура — использует системную клавиатуру Nintendo Switch (swkbd)

#include <string>

class Keyboard {
public:
    // Показывает системную клавиатуру и записывает результат в outText
    // Возвращает true, если пользователь подтвердил ввод
    static bool getText(const std::string& title, std::string& outText,
                        const std::string& initialText = "", int maxLength = 512);

    // Ввод только цифр
    static bool getNumber(const std::string& title, std::string& outText,
                          const std::string& initialText = "", int maxLength = 20);
};
