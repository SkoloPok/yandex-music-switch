// Yandex Music для Nintendo Switch
// Точка входа — инициализация всех подсистем и запуск главного цикла

#include <cstdio>
#include <cstdlib>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <curl/curl.h>

#include "app.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef __SWITCH__
    // === Инициализация сервисов Nintendo Switch ===

    // Сеть
    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);

    // Шрифты
    plInitialize(PlServiceType_User);

    // Файловая система romfs (для CA-сертификатов и ресурсов)
    romfsInit();

    // Предотвращаем засыпание во время воспроизведения музыки
    appletSetMediaPlaybackState(true);
#endif

    // === Инициализация SDL2 ===
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("IMG_Init failed: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Создание окна и рендерера
    SDL_Window* window = SDL_CreateWindow("Yandex Music",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN);

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Инициализация curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // === Создание и запуск приложения ===
    {
        App app(renderer);

        if (!app.init()) {
            printf("App init failed\n");
        } else {
            // Главный цикл
            while (app.isRunning()
#ifdef __SWITCH__
                   && appletMainLoop()
#endif
            ) {
                app.update();
                app.render();
            }
        }
    }

    // === Очистка (в обратном порядке) ===
    curl_global_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

#ifdef __SWITCH__
    appletSetMediaPlaybackState(false);
    romfsExit();
    plExit();
    nifmExit();
    socketExit();
#endif

    return 0;
}
