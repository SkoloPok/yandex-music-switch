// Реализация аудио плеера на SDL2_mixer для Nintendo Switch
// 48kHz стерео — аппаратное требование Switch

#include "player.h"

AudioPlayer* AudioPlayer::instance_ = nullptr;

AudioPlayer::AudioPlayer() {
    instance_ = this;
}

AudioPlayer::~AudioPlayer() {
    shutdown();
    instance_ = nullptr;
}

bool AudioPlayer::init() {
    // 48000 Hz — обязательно для Nintendo Switch
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        return false;
    }
    Mix_HookMusicFinished(onMusicFinished);
    return true;
}

void AudioPlayer::shutdown() {
    cleanupCurrent();
    Mix_CloseAudio();
}

bool AudioPlayer::playFromMemory(const std::vector<uint8_t>& audioData) {
    cleanupCurrent();

    if (audioData.empty()) return false;

    // Сохраняем данные в памяти — SDL_RWops ссылается на этот буфер
    currentAudioData_ = audioData;

    currentRWops_ = SDL_RWFromMem(currentAudioData_.data(),
                                   static_cast<int>(currentAudioData_.size()));
    if (!currentRWops_) return false;

    currentMusic_ = Mix_LoadMUS_RW(currentRWops_, 0); // 0 = не освобождать RWops автоматически
    if (!currentMusic_) {
        SDL_FreeRW(currentRWops_);
        currentRWops_ = nullptr;
        currentAudioData_.clear();
        return false;
    }

    if (Mix_PlayMusic(currentMusic_, 1) < 0) { // 1 = играть один раз
        cleanupCurrent();
        return false;
    }

    state_ = PlayerState::PLAYING;
    playStartTicks_ = SDL_GetTicks();
    pausedTicks_ = 0;
    pauseStartTicks_ = 0;

    return true;
}

void AudioPlayer::pause() {
    if (state_ == PlayerState::PLAYING) {
        Mix_PauseMusic();
        state_ = PlayerState::PAUSED;
        pauseStartTicks_ = SDL_GetTicks();
    }
}

void AudioPlayer::resume() {
    if (state_ == PlayerState::PAUSED) {
        Mix_ResumeMusic();
        state_ = PlayerState::PLAYING;
        if (pauseStartTicks_ > 0) {
            pausedTicks_ += SDL_GetTicks() - pauseStartTicks_;
            pauseStartTicks_ = 0;
        }
    }
}

void AudioPlayer::stop() {
    Mix_HaltMusic();
    state_ = PlayerState::STOPPED;
}

void AudioPlayer::next() {
    if (queue_.empty()) return;

    if (currentIndex_ + 1 < static_cast<int>(queue_.size())) {
        currentIndex_++;
        state_ = PlayerState::LOADING;
        if (needDownloadCb_) {
            needDownloadCb_(queue_[currentIndex_], currentIndex_);
        }
    } else {
        // Конец очереди
        stop();
    }
}

void AudioPlayer::previous() {
    if (queue_.empty()) return;

    // Если прошло > 3 секунд — перезапустить текущий трек
    if (getPositionMs() > 3000 && currentIndex_ >= 0) {
        state_ = PlayerState::LOADING;
        if (needDownloadCb_) {
            needDownloadCb_(queue_[currentIndex_], currentIndex_);
        }
        return;
    }

    if (currentIndex_ > 0) {
        currentIndex_--;
        state_ = PlayerState::LOADING;
        if (needDownloadCb_) {
            needDownloadCb_(queue_[currentIndex_], currentIndex_);
        }
    }
}

void AudioPlayer::setQueue(const std::vector<Track>& tracks, int startIndex) {
    queue_ = tracks;
    currentIndex_ = startIndex;
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(queue_.size())) {
        state_ = PlayerState::LOADING;
        if (needDownloadCb_) {
            needDownloadCb_(queue_[currentIndex_], currentIndex_);
        }
    }
}

void AudioPlayer::addToQueue(const Track& track) {
    queue_.push_back(track);
}

void AudioPlayer::clearQueue() {
    stop();
    queue_.clear();
    currentIndex_ = -1;
}

const Track* AudioPlayer::getCurrentTrack() const {
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(queue_.size())) {
        return &queue_[currentIndex_];
    }
    return nullptr;
}

float AudioPlayer::getProgress() const {
    int duration = getDurationMs();
    if (duration <= 0) return 0.0f;
    int pos = getPositionMs();
    float p = static_cast<float>(pos) / static_cast<float>(duration);
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

int AudioPlayer::getPositionMs() const {
    if (state_ == PlayerState::STOPPED || state_ == PlayerState::LOADING) return 0;

    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - playStartTicks_;

    // Вычитаем время в паузе
    uint32_t totalPaused = pausedTicks_;
    if (state_ == PlayerState::PAUSED && pauseStartTicks_ > 0) {
        totalPaused += now - pauseStartTicks_;
    }

    if (elapsed > totalPaused) {
        return static_cast<int>(elapsed - totalPaused);
    }
    return 0;
}

int AudioPlayer::getDurationMs() const {
    const Track* track = getCurrentTrack();
    if (track) return track->durationMs;
    return 0;
}

void AudioPlayer::update() {
    // Если трек закончился, Mix_Music finished callback уже вызвал onMusicFinished
}

void AudioPlayer::cleanupCurrent() {
    if (currentMusic_) {
        Mix_HaltMusic();
        Mix_FreeMusic(currentMusic_);
        currentMusic_ = nullptr;
    }
    if (currentRWops_) {
        SDL_FreeRW(currentRWops_);
        currentRWops_ = nullptr;
    }
    currentAudioData_.clear();
}

void AudioPlayer::onMusicFinished() {
    if (instance_ && instance_->state_ == PlayerState::PLAYING) {
        instance_->next();
    }
}
