#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class CVoiceCodec {
  public:
    CVoiceCodec();
    ~CVoiceCodec();

    CVoiceCodec(const CVoiceCodec&)                  = delete;
    CVoiceCodec&       operator=(const CVoiceCodec&) = delete;

    [[nodiscard]] bool encode(std::span<const float> samples, std::vector<std::uint8_t>& packet);
    [[nodiscard]] bool decode(std::span<const std::uint8_t> packet, std::vector<float>& samples);

  private:
    struct SImpl;
    std::unique_ptr<SImpl> mImpl;
};
