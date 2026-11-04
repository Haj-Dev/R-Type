#include "VoiceCodec.hpp"
#include "Shared/Types.hpp"
#include "Shared/Systems/AudioSystem.hpp"

#include <opus/opus.h>

#include <array>

struct CVoiceCodec::SImpl {
    OpusEncoder* encoder = nullptr;
    OpusDecoder* decoder = nullptr;
};

CVoiceCodec::CVoiceCodec() : mImpl(std::make_unique<SImpl>()) {
    int error = OPUS_OK;
    mImpl->encoder =
        opus_encoder_create(Audio::SampleRate, Audio::Channels, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK)
        mImpl->encoder = nullptr;
    error          = OPUS_OK;
    mImpl->decoder = opus_decoder_create(Audio::SampleRate, Audio::Channels, &error);
    if (error != OPUS_OK)
        mImpl->decoder = nullptr;
}

CVoiceCodec::~CVoiceCodec() {
    if (mImpl->encoder != nullptr)
        opus_encoder_destroy(mImpl->encoder);
    if (mImpl->decoder != nullptr)
        opus_decoder_destroy(mImpl->decoder);
}

bool CVoiceCodec::encode(std::span<const float> samples, std::vector<std::uint8_t>& packet) {
    if (mImpl->encoder == nullptr || samples.size() != Audio::SamplesPerFrame)
        return false;
    std::array<std::uint8_t, Net::MaxVoicePayloadSize> encoded{};
    const auto size = opus_encode_float(mImpl->encoder, samples.data(), static_cast<int>(samples.size()),
                                        encoded.data(), static_cast<opus_int32>(encoded.size()));
    if (size <= 0)
        return false;
    packet.assign(encoded.begin(), encoded.begin() + size);
    return true;
}

bool CVoiceCodec::decode(std::span<const std::uint8_t> packet, std::vector<float>& samples) {
    if (mImpl->decoder == nullptr || packet.empty() || packet.size() > Net::MaxVoicePayloadSize)
        return false;
    samples.resize(Audio::SamplesPerFrame);
    const auto decoded =
        opus_decode_float(mImpl->decoder, packet.data(), static_cast<opus_int32>(packet.size()),
                          samples.data(), static_cast<int>(samples.size()), 0);
    if (decoded <= 0)
        return false;
    samples.resize(static_cast<std::size_t>(decoded));
    return true;
}
