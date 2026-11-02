#include "AudioSystem.hpp"

#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

#include <miniaudio.h>

namespace Audio {
    namespace {

        class CMiniaudioBackend final : public IAudioBackend {
          public:
            ~CMiniaudioBackend() override {
                stop();
            }

            bool start() override {
                if (mStarted)
                    return true;

                auto config              = ma_device_config_init(ma_device_type_duplex);
                config.capture.format    = ma_format_f32;
                config.capture.channels  = Channels;
                config.playback.format   = ma_format_f32;
                config.playback.channels = Channels;
                config.sampleRate        = SampleRate;
                config.dataCallback      = &CMiniaudioBackend::callback;
                config.pUserData         = this;
                mDevice                  = std::make_unique<ma_device>();
                if (ma_device_init(nullptr, &config, mDevice.get()) != MA_SUCCESS) {
                    mDevice.reset();
                    return false;
                }
                if (ma_device_start(mDevice.get()) != MA_SUCCESS) {
                    ma_device_uninit(mDevice.get());
                    mDevice.reset();
                    return false;
                }
                mStarted = true;
                return true;
            }

            void stop() override {
                if (!mStarted)
                    return;
                ma_device_uninit(mDevice.get());
                mDevice.reset();
                std::scoped_lock lock(mMutex);
                mCapture.clear();
                mPlayback.clear();
                mStarted = false;
            }

            bool running() const override {
                return mStarted;
            }

            bool readCapture(std::span<float> frame) override {
                std::scoped_lock lock(mMutex);
                if (mCapture.size() < frame.size())
                    return false;
                std::ranges::copy_n(mCapture.begin(), frame.size(), frame.begin());
                mCapture.erase(mCapture.begin(),
                               mCapture.begin() + static_cast<std::ptrdiff_t>(frame.size()));
                return true;
            }

            void submitPlayback(std::span<const float> samples) override {
                constexpr std::size_t maxSamples = SampleRate / 2;
                std::scoped_lock      lock(mMutex);
                const auto            accepted =
                    std::min(samples.size(), maxSamples - std::min(maxSamples, mPlayback.size()));
                mPlayback.insert(mPlayback.end(), samples.begin(), samples.begin() + accepted);
            }

          private:
            static void callback(ma_device* device, void* output, const void* input,
                                 ma_uint32 frameCount) {
                auto*            backend = static_cast<CMiniaudioBackend*>(device->pUserData);
                std::scoped_lock lock(backend->mMutex);
                const auto*      captured = static_cast<const float*>(input);
                auto*            played   = static_cast<float*>(output);
                if (captured != nullptr)
                    backend->mCapture.insert(backend->mCapture.end(), captured, captured + frameCount);
                for (ma_uint32 index = 0; index < frameCount; ++index) {
                    if (played != nullptr)
                        played[index] = backend->mPlayback.empty() ? 0.0F : backend->mPlayback.front();
                    if (played != nullptr && !backend->mPlayback.empty())
                        backend->mPlayback.pop_front();
                }
                constexpr std::size_t maxSamples = SampleRate / 2;
                while (backend->mCapture.size() > maxSamples)
                    backend->mCapture.pop_front();
            }

            std::unique_ptr<ma_device> mDevice;
            bool                       mStarted = false;
            mutable std::mutex         mMutex;
            std::deque<float>          mCapture;
            std::deque<float>          mPlayback;
        };

    } // namespace

    std::unique_ptr<IAudioBackend> makeMiniaudioBackend() {
        return std::make_unique<CMiniaudioBackend>();
    }

} // namespace Audio
