// Реализация главного класса приложения
// Управляет экранами, обрабатывает глобальный ввод, координирует загрузку треков

#include "app.h"
#include "ui/screen.h"
#include "ui/login_screen.h"
#include "ui/home_screen.h"
#include "ui/search_screen.h"
#include "ui/playlist_screen.h"
#include "ui/player_bar.h"
#include "ui/theme.h"

App::App(SDL_Renderer* renderer)
    : sdlRenderer_(renderer) {}

App::~App() = default;

bool App::init() {
#ifdef __SWITCH__
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_state_);
#endif

    // Создаём подсистемы
    api_ = std::make_unique<YandexMusicAPI>();
    player_ = std::make_unique<AudioPlayer>();
    renderer_ = std::make_unique<Renderer>(sdlRenderer_);

    if (!renderer_->init()) {
        return false;
    }

    if (!player_->init()) {
        return false;
    }

    // Устанавливаем коллбэк загрузки трека
    player_->onNeedDownload([this](const Track& track, int /*index*/) {
        downloadAndPlayTrack(track);
    });

    // Создаём экраны
    loginScreen_ = std::make_unique<LoginScreen>(this);
    homeScreen_ = std::make_unique<HomeScreen>(this);
    searchScreen_ = std::make_unique<SearchScreen>(this);
    playlistScreen_ = std::make_unique<PlaylistScreen>(this);
    playerBar_ = std::make_unique<PlayerBar>(this);

    // Начинаем с экрана логина
    currentScreen_ = AppScreen::LOGIN;
    loginScreen_->onEnter();

    return true;
}

Screen* App::getActiveScreen() {
    switch (currentScreen_) {
        case AppScreen::LOGIN:    return loginScreen_.get();
        case AppScreen::HOME:     return homeScreen_.get();
        case AppScreen::SEARCH:   return searchScreen_.get();
        case AppScreen::PLAYLIST: return playlistScreen_.get();
    }
    return loginScreen_.get();
}

void App::navigateTo(AppScreen screen) {
    Screen* current = getActiveScreen();
    if (current) current->onExit();

    screenHistory_.push_back(currentScreen_);
    currentScreen_ = screen;

    Screen* next = getActiveScreen();
    if (next) next->onEnter();
}

void App::goBack() {
    if (screenHistory_.empty()) return;

    Screen* current = getActiveScreen();
    if (current) current->onExit();

    currentScreen_ = screenHistory_.back();
    screenHistory_.pop_back();

    Screen* prev = getActiveScreen();
    if (prev) prev->onEnter();
}

void App::update() {
    // Обработка SDL событий
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running_ = false;
                return;
            default:
                break;
        }
    }

    // Чтение ввода контроллера
    uint64_t kDown = 0;
    uint64_t kHeld = 0;

#ifdef __SWITCH__
    padUpdate(&pad_state_);
    kDown = padGetButtonsDown(&pad_state_);
    kHeld = padGetButtons(&pad_state_);
#else
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    static Uint8 prevKeys[SDL_NUM_SCANCODES] = {0};

    auto checkKey = [&](SDL_Scancode code, uint64_t btn) {
        if (keys[code]) {
            kHeld |= btn;
            if (!prevKeys[code]) kDown |= btn;
        }
    };

    checkKey(SDL_SCANCODE_RETURN, HidNpadButton_A);
    checkKey(SDL_SCANCODE_SPACE, HidNpadButton_A);
    checkKey(SDL_SCANCODE_ESCAPE, HidNpadButton_B);
    checkKey(SDL_SCANCODE_BACKSPACE, HidNpadButton_B);
    checkKey(SDL_SCANCODE_X, HidNpadButton_X);
    checkKey(SDL_SCANCODE_Y, HidNpadButton_Y);
    checkKey(SDL_SCANCODE_UP, HidNpadButton_Up);
    checkKey(SDL_SCANCODE_DOWN, HidNpadButton_Down);
    checkKey(SDL_SCANCODE_LEFT, HidNpadButton_Left);
    checkKey(SDL_SCANCODE_RIGHT, HidNpadButton_Right);
    checkKey(SDL_SCANCODE_1, HidNpadButton_L);
    checkKey(SDL_SCANCODE_2, HidNpadButton_R);
    checkKey(SDL_SCANCODE_TAB, HidNpadButton_R);
    checkKey(SDL_SCANCODE_P, HidNpadButton_Minus);
    checkKey(SDL_SCANCODE_MINUS, HidNpadButton_Minus);
    checkKey(SDL_SCANCODE_EQUALS, HidNpadButton_Plus);

    memcpy(prevKeys, keys, sizeof(prevKeys));
#endif

    // Глобальный ввод (работает на всех экранах)
    handleGlobalInput(kDown);

    // Ввод для активного экрана
    Screen* screen = getActiveScreen();
    if (screen) {
        screen->handleInput(kDown, kHeld);
        screen->update();
    }

    // Обновление плеера
    player_->update();
    playerBar_->update();
}

void App::render() {
    renderer_->clear();

    // Отрисовка активного экрана
    Screen* screen = getActiveScreen();
    if (screen) {
        screen->render();
    }

    // Панель плеера поверх всего
    if (playerBar_->isVisible()) {
        playerBar_->render();
    }

    renderer_->present();
}

void App::handleGlobalInput(uint64_t kDown) {
    // [+] — выход (на всех экранах кроме логина)
    if ((kDown & HidNpadButton_Plus) && currentScreen_ != AppScreen::LOGIN) {
        running_ = false;
    }

    // [-] — пауза/возобновление
    if (kDown & HidNpadButton_Minus) {
        if (player_->getState() == PlayerState::PLAYING) {
            player_->pause();
        } else if (player_->getState() == PlayerState::PAUSED) {
            player_->resume();
        }
    }

    // [ZL] — предыдущий трек
    if (kDown & HidNpadButton_ZL) {
        player_->previous();
    }

    // [ZR] — следующий трек
    if (kDown & HidNpadButton_ZR) {
        player_->next();
    }
}

void App::playTrack(const Track& track) {
    std::vector<Track> queue = {track};
    playPlaylist(queue, 0);
}

void App::playPlaylist(const std::vector<Track>& tracks, int startIndex) {
    player_->setQueue(tracks, startIndex);
    // setQueue вызовет needDownloadCb_, который вызовет downloadAndPlayTrack
}

void App::showPlaylist(const Playlist& playlist) {
    playlistScreen_->setPlaylist(playlist);
    navigateTo(AppScreen::PLAYLIST);
}

void App::downloadAndPlayTrack(const Track& track) {
    player_->setState(PlayerState::LOADING);

    // Скачиваем аудиоданные
    auto audioData = api_->downloadTrack(track.id);
    if (audioData.empty()) {
        player_->setState(PlayerState::STOPPED);
        return;
    }

    // Воспроизводим
    if (!player_->playFromMemory(audioData)) {
        player_->setState(PlayerState::STOPPED);
    }
}
