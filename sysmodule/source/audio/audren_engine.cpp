#include "audren_engine.h"
#include <cstring>
#include <cstdlib>
#include <chrono>

#ifdef __SWITCH__
#include <switch.h>

static constexpr size_t NUM_BUFFERS = 4;
static constexpr size_t SAMPLES_PER_BUF = 4096; // 4096 стерео-сэмплов
static constexpr size_t BYTES_PER_BUF = SAMPLES_PER_BUF * 2 * sizeof(int16_t); // 16384 байт (кратно 0x1000)

struct SwitchAudioContext {
    AudioDriver driver{};
    int voiceId = 0;
    int memPoolId = -1;
    uint8_t* memPoolBuffer = nullptr;
    size_t memPoolSize = 0;
    AudioDriverWaveBuf waveBufs[NUM_BUFFERS]{};
    int16_t* pcmBuffers[NUM_BUFFERS]{};
};
#endif

AudrenEngine::AudrenEngine() = default;

AudrenEngine::~AudrenEngine() {
    exit();
}

bool AudrenEngine::init() {
#ifdef __SWITCH__
    auto* ctx = new SwitchAudioContext();
    switchAudioData_ = ctx;

    static const AudioRendererConfig arConfig = {
        .output_rate     = AudioRendererOutputRate_48kHz,
        .num_voices      = 1,
        .num_effects     = 0,
        .num_sinks       = 1,
        .num_mix_objs    = 1,
    };

    Result rc = audrenInitialize(&arConfig);
    if (R_FAILED(rc)) {
        delete ctx;
        switchAudioData_ = nullptr;
        return false;
    }

    rc = audrvCreate(&ctx->driver, &arConfig, 2);
    if (R_FAILED(rc)) {
        audrenExit();
        delete ctx;
        switchAudioData_ = nullptr;
        return false;
    }

    ctx->memPoolSize = (BYTES_PER_BUF * NUM_BUFFERS + 0xFFF) & ~0xFFF;
    ctx->memPoolBuffer = static_cast<uint8_t*>(aligned_alloc(0x1000, ctx->memPoolSize));
    if (!ctx->memPoolBuffer) {
        audrvClose(&ctx->driver);
        audrenExit();
        delete ctx;
        switchAudioData_ = nullptr;
        return false;
    }
    std::memset(ctx->memPoolBuffer, 0, ctx->memPoolSize);

    armDCacheFlush(ctx->memPoolBuffer, ctx->memPoolSize);

    ctx->memPoolId = audrvMemPoolAdd(&ctx->driver, ctx->memPoolBuffer, ctx->memPoolSize);
    audrvMemPoolAttach(&ctx->driver, ctx->memPoolId);

    for (size_t i = 0; i < NUM_BUFFERS; ++i) {
        ctx->pcmBuffers[i] = reinterpret_cast<int16_t*>(ctx->memPoolBuffer + i * BYTES_PER_BUF);
        ctx->waveBufs[i].data_raw = ctx->pcmBuffers[i];
        ctx->waveBufs[i].size = BYTES_PER_BUF;
        ctx->waveBufs[i].start_sample_offset = 0;
        ctx->waveBufs[i].end_sample_offset = SAMPLES_PER_BUF;
    }

    ctx->voiceId = 0;
    audrvVoiceInit(&ctx->driver, ctx->voiceId, 2, PcmFormat_Int16, 44100);
    audrvVoiceSetDestinationMix(&ctx->driver, ctx->voiceId, AUDREN_FINAL_MIX_ID);
    audrvVoiceSetVolume(&ctx->driver, ctx->voiceId, volume_ / 100.0f);

    audrvUpdate(&ctx->driver);
    audrenStartAudioRenderer();
#endif
    return true;
}

void AudrenEngine::exit() {
    stop();

#ifdef __SWITCH__
    if (switchAudioData_) {
        auto* ctx = static_cast<SwitchAudioContext*>(switchAudioData_);
        audrvVoiceStop(&ctx->driver, ctx->voiceId);
        audrvClose(&ctx->driver);
        audrenExit();
        if (ctx->memPoolBuffer) {
            free(ctx->memPoolBuffer);
        }
        delete ctx;
        switchAudioData_ = nullptr;
    }
#endif
}

void AudrenEngine::playStream(RingBuffer* ringBuffer) {
    stop();

    currentRingBuffer_ = ringBuffer;
    decoder_.reset();
    elapsedSeconds_ = 0;
    isPaused_ = false;
    isPlaying_ = true;
    threadRunning_ = true;

    audioThread_ = std::thread(&AudrenEngine::audioThreadLoop, this);
}

void AudrenEngine::setPaused(bool paused) {
    isPaused_ = paused;
#ifdef __SWITCH__
    if (switchAudioData_) {
        auto* ctx = static_cast<SwitchAudioContext*>(switchAudioData_);
        audrvVoiceSetPaused(&ctx->driver, ctx->voiceId, paused);
    }
#endif
}

void AudrenEngine::stop() {
    if (threadRunning_) {
        threadRunning_ = false;
        if (audioThread_.joinable()) {
            audioThread_.join();
        }
    }
    isPlaying_ = false;
    isPaused_ = false;
    currentRingBuffer_ = nullptr;

#ifdef __SWITCH__
    if (switchAudioData_) {
        auto* ctx = static_cast<SwitchAudioContext*>(switchAudioData_);
        audrvVoiceStop(&ctx->driver, ctx->voiceId);
        audrvVoiceDrop(&ctx->driver, ctx->voiceId);
    }
#endif
}

void AudrenEngine::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    volume_ = volume;

#ifdef __SWITCH__
    if (switchAudioData_) {
        auto* ctx = static_cast<SwitchAudioContext*>(switchAudioData_);
        audrvVoiceSetVolume(&ctx->driver, ctx->voiceId, volume / 100.0f);
    }
#endif
}

void AudrenEngine::audioThreadLoop() {
#ifdef __SWITCH__
    auto* ctx = static_cast<SwitchAudioContext*>(switchAudioData_);
    if (!ctx) return;

    bool voiceStarted = false;
    std::vector<uint8_t> mp3Chunk(4096);
    int16_t framePcm[Mp3Decoder::MAX_SAMPLES_PER_FRAME * 2];
    uint64_t totalSamplesDecoded = 0;
    int currentSampleRate = 44100;

    while (threadRunning_) {
        if (isPaused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Ждём кадра аудиорендерера
        audrenWaitFrame();
        audrvUpdate(&ctx->driver);

        // Проверяем буферы
        for (size_t b = 0; b < NUM_BUFFERS && threadRunning_; ++b) {
            auto* wb = &ctx->waveBufs[b];

            // Если буфер свободен для записи
            if (wb->state == AudioDriverWaveBufState_Free || wb->state == AudioDriverWaveBufState_Done) {
                int16_t* dest = ctx->pcmBuffers[b];
                size_t samplesFilled = 0;

                while (samplesFilled < SAMPLES_PER_BUF && threadRunning_) {
                    // Смотрим, есть ли данные в ring buffer
                    size_t available = currentRingBuffer_ ? currentRingBuffer_->size() : 0;
                    if (available < 4) {
                        // Не хватает данных потока
                        if (currentRingBuffer_ && currentRingBuffer_->isStopped()) {
                            // Поток завершён
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    size_t toPeek = std::min(available, mp3Chunk.size());
                    currentRingBuffer_->peek(mp3Chunk.data(), toPeek);

                    DecodedFrame frame = decoder_.decodeFrame(mp3Chunk.data(), toPeek, framePcm, Mp3Decoder::MAX_SAMPLES_PER_FRAME * 2);

                    if (frame.bytesConsumed > 0) {
                        currentRingBuffer_->advance(frame.bytesConsumed);
                    } else {
                        // Пропускаем 1 байт в поисках синхрослока
                        currentRingBuffer_->advance(1);
                        continue;
                    }

                    if (frame.samplesPerChannel > 0) {
                        if (frame.sampleRate > 0 && frame.sampleRate != currentSampleRate) {
                            currentSampleRate = frame.sampleRate;
                            audrvVoiceSetPitch(&ctx->driver, ctx->voiceId, static_cast<float>(currentSampleRate) / 44100.0f);
                        }

                        size_t toCopy = std::min(static_cast<size_t>(frame.samplesPerChannel), SAMPLES_PER_BUF - samplesFilled);
                        if (frame.channels == 2) {
                            std::memcpy(dest + samplesFilled * 2, framePcm, toCopy * 2 * sizeof(int16_t));
                        } else {
                            // Моно в стерео
                            for (size_t s = 0; s < toCopy; ++s) {
                                dest[(samplesFilled + s) * 2] = framePcm[s];
                                dest[(samplesFilled + s) * 2 + 1] = framePcm[s];
                            }
                        }
                        samplesFilled += toCopy;
                        totalSamplesDecoded += toCopy;
                        elapsedSeconds_ = static_cast<int>(totalSamplesDecoded / currentSampleRate);
                    }
                }

                if (samplesFilled > 0) {
                    // Заполняем остаток нулями, если трек кончился
                    if (samplesFilled < SAMPLES_PER_BUF) {
                        std::memset(dest + samplesFilled * 2, 0, (SAMPLES_PER_BUF - samplesFilled) * 2 * sizeof(int16_t));
                    }

                    armDCacheFlush(dest, BYTES_PER_BUF);

                    wb->size = BYTES_PER_BUF;
                    wb->end_sample_offset = SAMPLES_PER_BUF;
                    audrvVoiceAddWaveBuf(&ctx->driver, ctx->voiceId, wb);

                    if (!voiceStarted) {
                        audrvVoiceStart(&ctx->driver, ctx->voiceId);
                        voiceStarted = true;
                    }
                }
            }
        }
    }
#else
    while (threadRunning_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
#endif
}
