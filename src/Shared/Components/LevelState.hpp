#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstdint>
#include <vector>

struct SCLevelState : AComponent {
    SCLevelState() = default;
    SCLevelState(std::uint8_t currentLevel, std::uint8_t levelCount, float transitionSeconds = 0.0F,
                 bool campaignFinished = false) :
        level(currentLevel), totalLevels(levelCount), transitionRemaining(transitionSeconds),
        finished(campaignFinished) {}

    explicit SCLevelState(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        std::uint8_t       finishedByte = 0;
        if (!reader.read(level) || !reader.read(totalLevels) || !reader.read(transitionRemaining) ||
            !reader.read(finishedByte) || !reader.finished() || level == 0 || totalLevels == 0 ||
            level > totalLevels || finishedByte > 1 || transitionRemaining < 0.0F) {
            setInvalid();
            return;
        }
        finished = finishedByte != 0;
    }

    [[nodiscard]] eType type() const override {
        return eType::LEVEL_STATE;
    }

    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, level);
        Components::append(data, totalLevels);
        Components::append(data, transitionRemaining);
        Components::append(data, static_cast<std::uint8_t>(finished));
    }

    std::uint8_t level = 1;
    std::uint8_t totalLevels = 1;
    float transitionRemaining = 0.0F;
    bool finished = false;
};
