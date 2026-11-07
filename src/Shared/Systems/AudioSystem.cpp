#include "AudioSystem.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Audio {

    AAudioSystem::AAudioSystem(std::unique_ptr<IAudioBackend> backend) : mBackend(std::move(backend)) {
        if (mBackend == nullptr)
            throw std::invalid_argument("audio backend must not be null");
        mVolumes.fill(1.0F);
        mMuted.fill(false);
    }

    AAudioSystem::~AAudioSystem() {
        stop();
    }

    bool AAudioSystem::start() {
        return mBackend->start();
    }

    void AAudioSystem::stop() {
        mBackend->stop();
    }

    bool AAudioSystem::running() const {
        return mBackend->running();
    }

    void AAudioSystem::playVoice(std::uint8_t playerId, std::span<const float> samples) {
        if (playerId >= MaxPlayers || samples.empty() || !running() || mMuted.at(playerId))
            return;

        const auto         volume = mVolumes.at(playerId);
        std::vector<float> scaled(samples.begin(), samples.end());
        for (auto& sample : scaled)
            sample *= volume;
        mBackend->submitPlayback(scaled);
    }

    bool AAudioSystem::captureVoice(std::span<float> frame) {
        if (frame.size() != SamplesPerFrame || !running())
            return false;
        return mBackend->readCapture(frame);
    }

    void AAudioSystem::setPlayerVolume(std::uint8_t playerId, float volume) {
        if (playerId >= MaxPlayers)
            return;
        mVolumes.at(playerId) = std::clamp(volume, 0.0F, 1.0F);
    }

    void AAudioSystem::setPlayerMuted(std::uint8_t playerId, bool muted) {
        if (playerId < MaxPlayers)
            mMuted.at(playerId) = muted;
    }

} // namespace Audio
