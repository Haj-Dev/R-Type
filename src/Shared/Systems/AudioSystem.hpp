#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Audio {

    inline constexpr std::uint32_t SampleRate        = 48'000;
    inline constexpr std::uint32_t Channels          = 1;
    inline constexpr std::uint32_t FrameMilliseconds = 20;
    inline constexpr std::size_t   SamplesPerFrame   = SampleRate * FrameMilliseconds / 1'000;
    inline constexpr std::size_t   MaxPlayers        = 4;

    class IAudioBackend {
      public:
        virtual ~IAudioBackend() = default;

        [[nodiscard]] virtual bool start()                                        = 0;
        virtual void               stop()                                         = 0;
        [[nodiscard]] virtual bool running() const                                = 0;
        [[nodiscard]] virtual bool readCapture(std::span<float> frame)            = 0;
        virtual void               submitPlayback(std::span<const float> samples) = 0;
    };

    // Owns voice policy and audio mixing while leaving device access to a backend.
    class AAudioSystem {
      public:
        explicit AAudioSystem(std::unique_ptr<IAudioBackend> backend);
        ~AAudioSystem();

        AAudioSystem(const AAudioSystem&)                 = delete;
        AAudioSystem&      operator=(const AAudioSystem&) = delete;

        [[nodiscard]] bool start();
        void               stop();
        [[nodiscard]] bool running() const;

        // Called by the voice transport once an encoded frame has been decoded.
        void playVoice(std::uint8_t playerId, std::span<const float> samples);

        // Called by the voice transport to obtain one microphone frame.
        [[nodiscard]] bool captureVoice(std::span<float> frame);

        void               setPlayerVolume(std::uint8_t playerId, float volume);
        void               setPlayerMuted(std::uint8_t playerId, bool muted);

      private:
        std::unique_ptr<IAudioBackend> mBackend;
        std::array<float, MaxPlayers>  mVolumes{};
        std::array<bool, MaxPlayers>   mMuted{};
    };

    // Creates the platform audio backend. Keeping this factory here prevents
    // the client, networking, and UI code from depending on miniaudio.
    [[nodiscard]] std::unique_ptr<IAudioBackend> makeMiniaudioBackend();

} // namespace Audio
