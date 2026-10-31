#include "mp3_decoder.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "../../include/minimp3.h"

Mp3Decoder::Mp3Decoder() {
    mp3dec_t* dec = new mp3dec_t;
    mp3dec_init(dec);
    decoder_ = dec;
}

Mp3Decoder::~Mp3Decoder() {
    delete static_cast<mp3dec_t*>(decoder_);
}

void Mp3Decoder::reset() {
    if (decoder_) {
        mp3dec_init(static_cast<mp3dec_t*>(decoder_));
    }
}

DecodedFrame Mp3Decoder::decodeFrame(const uint8_t* mp3Data, size_t mp3Size, int16_t* outPcm, size_t maxSamples) {
    DecodedFrame result{};
    if (!decoder_ || !mp3Data || mp3Size < 4 || !outPcm || maxSamples < MINIMP3_MAX_SAMPLES_PER_FRAME) {
        return result;
    }

    mp3dec_t* dec = static_cast<mp3dec_t*>(decoder_);
    mp3dec_frame_info_t info{};
    int samples = mp3dec_decode_frame(dec, mp3Data, static_cast<int>(mp3Size), outPcm, &info);

    result.bytesConsumed = info.frame_bytes;
    result.channels = info.channels;
    result.sampleRate = info.hz;
    result.samplesPerChannel = samples;

    return result;
}
