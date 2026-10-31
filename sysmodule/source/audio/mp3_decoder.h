#pragma once

#include <cstdint>
#include <cstddef>

struct DecodedFrame {
    int samplesPerChannel = 0;
    int channels = 0;
    int sampleRate = 0;
    int bytesConsumed = 0;
};

class Mp3Decoder {
public:
    static constexpr size_t MAX_SAMPLES_PER_FRAME = 1152;

    Mp3Decoder();
    ~Mp3Decoder();

    void reset();

    // Декодирует один MP3 фрейм в буфер outPcm (16-битный PCM, interleaved stereo)
    DecodedFrame decodeFrame(const uint8_t* mp3Data, size_t mp3Size, int16_t* outPcm, size_t maxSamples);

private:
    void* decoder_ = nullptr;
};
