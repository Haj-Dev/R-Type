#pragma once

#include <array>
#include <map>

#include "Shared/Components/Abstract/AComponent.hpp"

// Numbers are in ticks, 60 times per second

struct SUpdateConfig {
    const uint64_t interval;
    const uint64_t offset;
};

/// The snapshot policy defines how often the server sends a snapshot of the game state to the clients.
const SUpdateConfig SNAPSHOT_POLICY = {.interval = 60, .offset = 0};

/// The snapshot components define which components are included in the snapshot sent to the clients.
const std::array<AComponent::eType, 11> SNAPSHOT_COMPONENTS = {
    AComponent::eType::POSITION,
    AComponent::eType::VELOCITY,
    AComponent::eType::HEALTH,
    AComponent::eType::PLAYER,
    AComponent::eType::PROJECTILE,
    AComponent::eType::COLLIDER,
    AComponent::eType::DRAWABLE_SHAPE,
    AComponent::eType::DRAWABLE_SPRITE,
    AComponent::eType::DRAWABLE_SPRITE_ATLAS,
    AComponent::eType::ENTITY_TYPE,
    AComponent::eType::LEVEL_STATE,
};

// NOLINTBEGIN

/// The component update policy defines how often the server sends updates for each component type to the clients.
/// Component not in this list will not be udpated via updates
inline const std::map<AComponent::eType, SUpdateConfig> COMPONENT_UPDATE_POLICY = {
    {AComponent::eType::POSITION, {.interval = 2, .offset = 0}},
    {AComponent::eType::VELOCITY, {.interval = 10, .offset = 1}},
    {AComponent::eType::PLAYER, {.interval = 20, .offset = 4}},
    {AComponent::eType::HEALTH, {.interval = 20, .offset = 13}},
    {AComponent::eType::LEVEL_STATE, {.interval = 20, .offset = 0}},
};
// NOLINTEND

/// number of ticks between sending the entity creation packet to ensure it is received by the client
const uint64_t ENTITY_CREATION_RESEND_INTERVAL = 5;
/// number of times to send the entity creation packet to ensure it is received by the client
const uint64_t ENTITY_CREATION_RESEND_REPEAT = 6;

/// number of ticks between sending the entity update packet to ensure it is received by the client
const uint64_t ENTITY_DESTRUCTION_RESEND_INTERVAL = 5;
/// number of times to send the entity update packet to ensure it is received by the client
const uint64_t ENTITY_DESTRUCTION_RESEND_REPEAT = 6;