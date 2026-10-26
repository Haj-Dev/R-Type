#pragma once

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Shared/Components/ComponentRegistry.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Net {

    inline constexpr std::uint16_t               DefaultTcpPort = 7000;
    inline constexpr std::uint16_t               DefaultUdpPort = 7001;
    inline constexpr std::array<std::uint8_t, 3> Magic{{'H', 'A', 'J'}};
    inline constexpr std::uint16_t               Terminator = 0x0D0A;

    enum class ePacketType : std::uint8_t {
        HELLO    = 1,
        INPUT    = 2,
        SNAPSHOT = 3,
    };

    enum class eCommandType : std::uint8_t {
        HELLO           = 1,
        INPUT           = 2,
        WELCOME         = 3,
        SNAPSHOT_ENTITY = 4,
    };

    inline constexpr std::size_t HeaderSize        = 4;
    inline constexpr std::size_t CommandHeaderSize = 9;
    inline constexpr std::size_t HandshakeSize     = 7;
    inline constexpr std::size_t MaxUdpPayloadSize = 65507;

    inline void appendBytes(std::vector<std::uint8_t>& output, const void* data, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        output.insert(output.end(), bytes, bytes + size);
    }

    template <typename T>
    inline void append(std::vector<std::uint8_t>& output, const T& value) {
        appendBytes(output, &value, sizeof(T));
    }

    inline void appendCommand(std::vector<std::uint8_t>& packet, eCommandType command,
                              std::uint64_t entityId, const void* payload, std::size_t payloadSize) {
        append(packet, command);
        append(packet, entityId);
        appendBytes(packet, payload, payloadSize);
    }

    inline std::vector<std::uint8_t> beginPacket(ePacketType type) {
        return {Magic.at(0), Magic.at(1), Magic.at(2), static_cast<std::uint8_t>(type)};
    }

    inline void finishPacket(std::vector<std::uint8_t>& packet) {
        packet.push_back(0x0D);
        packet.push_back(0x0A);
    }

    inline std::vector<std::uint8_t> makeHello() {
        auto packet = beginPacket(ePacketType::HELLO);
        appendCommand(packet, eCommandType::HELLO, 0, nullptr, 0);
        finishPacket(packet);
        return packet;
    }

    inline std::vector<std::uint8_t> makeTcpHello(std::uint16_t udpPort) {
        const auto port = static_cast<std::uint32_t>(udpPort);
        return {
            Magic.at(0),
            Magic.at(1),
            Magic.at(2),
            static_cast<std::uint8_t>(ePacketType::HELLO),
            static_cast<std::uint8_t>(port & 0xffU),
            static_cast<std::uint8_t>((port >> 8U) & 0xffU),
            0x0D,
        };
    }

    inline bool readTcpHello(const std::uint8_t* data, std::size_t size, std::uint16_t& udpPort) {
        if (size != HandshakeSize || data[0] != Magic.at(0) || data[1] != Magic.at(1) ||
            data[2] != Magic.at(2) || data[3] != static_cast<std::uint8_t>(ePacketType::HELLO) ||
            data[6] != 0x0D)
            return false;
        udpPort = static_cast<std::uint16_t>(data[4]) | (static_cast<std::uint32_t>(data[5]) << 8U);
        return udpPort != 0;
    }

    inline std::vector<std::uint8_t> makeTcpWelcome(std::uint8_t playerId) {
        return {
            Magic.at(0), Magic.at(1), Magic.at(2), static_cast<std::uint8_t>(eCommandType::WELCOME),
            playerId,
        };
    }

    inline bool readTcpWelcome(const std::uint8_t* data, std::size_t size, std::uint8_t& playerId) {
        if (size != 5 || data[0] != Magic.at(0) || data[1] != Magic.at(1) || data[2] != Magic.at(2) ||
            data[3] != static_cast<std::uint8_t>(eCommandType::WELCOME))
            return false;
        playerId = data[4];
        return playerId < SPLayerActions::MaxPlayers;
    }

    inline std::vector<std::uint8_t> makeInput(const SPlayerActionState& action) {
        auto                packet = beginPacket(ePacketType::INPUT);
        const std::uint32_t flags  = static_cast<std::uint32_t>(action.up) |
            (static_cast<std::uint32_t>(action.down) << 1U) |
            (static_cast<std::uint32_t>(action.left) << 2U) |
            (static_cast<std::uint32_t>(action.right) << 3U) |
            (static_cast<std::uint32_t>(action.fire) << 4U);
        const auto encodedFlags = static_cast<std::uint8_t>(flags);
        appendCommand(packet, eCommandType::INPUT, 0, &encodedFlags, sizeof(encodedFlags));
        finishPacket(packet);
        return packet;
    }

    inline bool validPacket(const std::uint8_t* data, std::size_t size, ePacketType expected) {
        const std::vector<std::uint8_t> bytes(data, data + size);
        return size >= HeaderSize + CommandHeaderSize + sizeof(Terminator) &&
            bytes.at(0) == Magic.at(0) && bytes.at(1) == Magic.at(1) && bytes.at(2) == Magic.at(2) &&
            bytes.at(3) == static_cast<std::uint8_t>(expected) && bytes.at(size - 2) == 0x0D &&
            bytes.at(size - 1) == 0x0A;
    }

    inline bool readInput(const std::uint8_t* data, std::size_t size, SPlayerActionState& action) {
        if (!validPacket(data, size, ePacketType::INPUT) ||
            data[HeaderSize] != static_cast<std::uint8_t>(eCommandType::INPUT) ||
            size != HeaderSize + CommandHeaderSize + sizeof(std::uint8_t) + sizeof(Terminator))
            return false;
        const std::vector<std::uint8_t> bytes(data, data + size);
        const auto flags = static_cast<std::uint32_t>(bytes.at(HeaderSize + CommandHeaderSize));
        action           = SPlayerActionState{
                      .up    = (flags & 1U) != 0,
                      .down  = (flags & 2U) != 0,
                      .left  = (flags & 4U) != 0,
                      .right = (flags & 8U) != 0,
                      .fire  = (flags & 16U) != 0,
        };
        return true;
    }

    inline std::vector<std::uint8_t> makeWelcome(std::uint8_t playerId) {
        auto packet = beginPacket(ePacketType::HELLO);
        appendCommand(packet, eCommandType::WELCOME, 0, &playerId, sizeof(playerId));
        finishPacket(packet);
        return packet;
    }

    inline bool readWelcome(const std::uint8_t* data, std::size_t size, std::uint8_t& playerId) {
        if (!validPacket(data, size, ePacketType::HELLO) ||
            data[HeaderSize] != static_cast<std::uint8_t>(eCommandType::WELCOME) ||
            size != HeaderSize + CommandHeaderSize + sizeof(playerId) + sizeof(Terminator))
            return false;
        const std::vector<std::uint8_t> bytes(data, data + size);
        playerId = bytes.at(HeaderSize + CommandHeaderSize);
        return playerId < SPLayerActions::MaxPlayers;
    }

    inline std::vector<std::uint8_t> makeSnapshot(const SSceneData& scene) {
        auto packet = beginPacket(ePacketType::SNAPSHOT);
        for (const auto& entity : scene.entities) {
            std::vector<std::uint8_t> payload;
            const auto                appendComponent = [&payload](const AComponent& component) {
                std::vector<std::uint8_t> bytes;
                component.serialize(bytes);
                append(payload, static_cast<std::uint32_t>(bytes.size()));
                payload.insert(payload.end(), bytes.begin(), bytes.end());
            };
            append(payload, static_cast<std::uint16_t>(4U));
            appendComponent(SCPosition{entity.x, entity.y});
            appendComponent(SCEntityType{entity.kind});
            appendComponent(SCHealth{entity.health});
            if (entity.texture.empty()) {
                appendComponent(SCDrawableShape{eDrawableShape::CIRCLE,
                                                entity.kind == Game::eEntityKind::PLAYER ? 18.0F : 8.0F,
                                                static_cast<std::uint8_t>(entity.kind), entity.layer});
            } else if (entity.atlas) {
                appendComponent(SCDrawableSpriteAtlas{
                    entity.texture, entity.baseDimension, entity.frames, entity.frameDuration,
                    entity.variant, entity.layer, entity.scale,
                    SCPosition{entity.offsetX, entity.offsetY}, entity.rotation});
            } else {
                appendComponent(SCDrawableSprite{entity.texture, entity.layer, entity.scale,
                                                 SCPosition{entity.offsetX, entity.offsetY},
                                                 entity.rotation});
            }
            std::vector<std::uint8_t> framedPayload;
            append(framedPayload, static_cast<std::uint32_t>(payload.size()));
            framedPayload.insert(framedPayload.end(), payload.begin(), payload.end());
            appendCommand(packet, eCommandType::SNAPSHOT_ENTITY, entity.id, framedPayload.data(),
                          framedPayload.size());
        }
        finishPacket(packet);
        return packet;
    }

    inline bool readSnapshot(const std::uint8_t* data, std::size_t size,
                             std::vector<Game::SSnapshotEntity>& entities) {
        if (!validPacket(data, size, ePacketType::SNAPSHOT))
            return false;
        CComponentRegistry registry;
        entities.clear();
        std::size_t offset = HeaderSize;
        while (offset < size - sizeof(Terminator)) {
            if (offset + CommandHeaderSize + sizeof(std::uint32_t) > size - sizeof(Terminator) ||
                data[offset] != static_cast<std::uint8_t>(eCommandType::SNAPSHOT_ENTITY))
                return false;
            std::uint64_t entityId = 0;
            std::memcpy(&entityId, data + offset + 1, sizeof(entityId));
            offset += CommandHeaderSize;
            std::uint32_t payloadSize = 0;
            std::memcpy(&payloadSize, data + offset, sizeof(payloadSize));
            offset += sizeof(payloadSize);
            if (payloadSize > size - sizeof(Terminator) - offset)
                return false;
            const auto payloadEnd = offset + payloadSize;
            if (offset + sizeof(std::uint16_t) > payloadEnd)
                return false;
            std::uint16_t componentCount = 0;
            std::memcpy(&componentCount, data + offset, sizeof(componentCount));
            offset += sizeof(componentCount);
            Game::SSnapshotEntity entity;
            entity.id = static_cast<std::uint32_t>(entityId);
            for (std::uint16_t index = 0; index < componentCount; ++index) {
                if (offset + sizeof(std::uint32_t) > payloadEnd)
                    return false;
                std::uint32_t componentSize = 0;
                std::memcpy(&componentSize, data + offset, sizeof(componentSize));
                offset += sizeof(componentSize);
                if (componentSize > payloadEnd - offset)
                    return false;
                const std::vector<std::uint8_t> bytes(data + offset, data + offset + componentSize);
                offset += componentSize;
                auto component = registry.deserialize(bytes);
                if (!component)
                    return false;
                if (const auto* position = dynamic_cast<const SCPosition*>(component.get())) {
                    entity.x = position->x;
                    entity.y = position->y;
                } else if (const auto* type = dynamic_cast<const SCEntityType*>(component.get())) {
                    entity.kind = type->value;
                } else if (const auto* health = dynamic_cast<const SCHealth*>(component.get())) {
                    entity.health = health->value;
                } else if (const auto* shape = dynamic_cast<const SCDrawableShape*>(component.get())) {
                    entity.texture.clear();
                    entity.layer = shape->layer;
                } else if (const auto* atlas =
                               dynamic_cast<const SCDrawableSpriteAtlas*>(component.get())) {
                    entity.texture       = atlas->texture;
                    entity.layer         = atlas->layer;
                    entity.scale         = atlas->scale;
                    entity.offsetX       = atlas->offset.x;
                    entity.offsetY       = atlas->offset.y;
                    entity.rotation      = atlas->rotation;
                    entity.atlas         = true;
                    entity.baseDimension = atlas->baseDimension;
                    entity.frames        = atlas->frames;
                    entity.frameDuration = atlas->frameDuration;
                    entity.variant       = atlas->variant;
                } else if (const auto* sprite = dynamic_cast<const SCDrawableSprite*>(component.get())) {
                    entity.texture  = sprite->texture;
                    entity.layer    = sprite->layer;
                    entity.scale    = sprite->scale;
                    entity.offsetX  = sprite->offset.x;
                    entity.offsetY  = sprite->offset.y;
                    entity.rotation = sprite->rotation;
                }
            }
            if (offset != payloadEnd)
                return false;
            entities.push_back(std::move(entity));
            offset = payloadEnd;
        }
        return offset == size - sizeof(Terminator);
    }

} // namespace Net
