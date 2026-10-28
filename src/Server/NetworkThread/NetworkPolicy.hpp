#pragma once

#include <map>

#include "../../Shared/Components/Abstract/AComponent.hpp"

// Numbers are in ticks, 60 times per second

struct SUpdateConfig {
    const uint64_t interval;
    const uint64_t offset;
};

const SUpdateConfig SNAPSHOT_POLICY = {.interval = 60, .offset = 0};

// Component not in this list will not be udpated via updates

// NOLINTBEGIN
inline const std::map<AComponent::eType, SUpdateConfig> COMPONENT_UPDATE_INTERVALS = {
    {AComponent::eType::POSITION, {.interval = 1, .offset = 0}},
    {AComponent::eType::VELOCITY, {.interval = 1, .offset = 0}},
    {AComponent::eType::PLAYER, {.interval = 5, .offset = 0}},
    {AComponent::eType::HEALTH, {.interval = 5, .offset = 1}},
};
// NOLINTEND