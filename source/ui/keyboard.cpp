#include "keyboard.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

bool Keyboard::getText(const std::string& title, std::string& outText,
                       const std::string& initialText, int maxLength) {
#ifdef __SWITCH__
    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, title.c_str());
    swkbdConfigSetGuideText(&kbd, title.c_str());

    if (!initialText.empty()) {
        swkbdConfigSetInitialText(&kbd, initialText.c_str());
    }

    swkbdConfigSetStringLenMax(&kbd, maxLength);

    char text[513] = {0};
    Result rc = swkbdShow(&kbd, text, sizeof(text));
    swkbdClose(&kbd);

    if (R_SUCCEEDED(rc)) {
        outText = text;
        return true;
    }
    return false;
#else
    // Для отладки вне Switch
    if (!initialText.empty()) {
        outText = initialText;
        return true;
    }
    printf("[Keyboard Prompt] %s: ", title.c_str());
    char buf[512] = {0};
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        outText = buf;
        return !outText.empty();
    }
    return false;
#endif
}

bool Keyboard::getNumber(const std::string& title, std::string& outText,
                         const std::string& initialText, int maxLength) {
#ifdef __SWITCH__
    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, title.c_str());
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);

    if (!initialText.empty()) {
        swkbdConfigSetInitialText(&kbd, initialText.c_str());
    }

    swkbdConfigSetStringLenMax(&kbd, maxLength);

    char text[64] = {0};
    Result rc = swkbdShow(&kbd, text, sizeof(text));
    swkbdClose(&kbd);

    if (R_SUCCEEDED(rc)) {
        outText = text;
        return true;
    }
    return false;
#else
    outText = initialText;
    return true;
#endif
}
