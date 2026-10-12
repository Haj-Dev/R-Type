#pragma once

#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"

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
            std::array<std::uint8_t, 11> payload{};
            std::memcpy(payload.data(), &entity.kind, sizeof(entity.kind));
            std::memcpy(payload.data() + 1, &entity.x, sizeof(entity.x));
            std::memcpy(payload.data() + 5, &entity.y, sizeof(entity.y));
            std::memcpy(payload.data() + 9, &entity.health, sizeof(entity.health));
            appendCommand(packet, eCommandType::SNAPSHOT_ENTITY, entity.id, payload.data(),
                          payload.size());
        }
        finishPacket(packet);
        return packet;
    }

} // namespace Net
