#pragma once

#include "Shared/Lobby.hpp"
#include "Shared/Types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Net {

    // Lobby frames reuse the control-plane framing: magic, command, u16 payload length.
    // Multi-byte integers are big-endian, unlike the UDP gameplay protocol.
    inline constexpr std::size_t   LobbyHeaderSize = 6;
    inline constexpr std::uint16_t MaxLobbyPayload = 4096;

    enum class eLobbyCommand : std::uint8_t {
        JOIN              = 30,
        ROOM_LIST         = 31,
        CREATE_ROOM       = 32,
        SELECT_ROOM       = 33,
        ROOM_JOINED       = 34,
        ROOM_STATE        = 35,
        ROOM_REJECTED     = 36,
        LAUNCH_GAME       = 37,
        GAME_STARTED      = 38,
        LEAVE_ROOM        = 39,
        ROOM_LEFT         = 40,
        ROOM_LIST_REQUEST = 41,
        CHAT_SEND         = 42,
        CHAT_MESSAGE      = 43,
    };

    inline void appendU16BE(std::vector<std::uint8_t>& output, std::uint16_t value) {
        output.push_back(static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 8U) & 0xFFU));
        output.push_back(static_cast<std::uint8_t>(static_cast<unsigned>(value) & 0xFFU));
    }

    [[nodiscard]] inline std::uint16_t readU16BE(const std::uint8_t* data) {
        return static_cast<std::uint16_t>((static_cast<unsigned>(data[0]) << 8U) |
                                          static_cast<unsigned>(data[1]));
    }

    [[nodiscard]] inline bool validName(const std::string& name) {
        if (name.empty() || name.size() > Room::MaxPlayerNameLength)
            return false;
        return std::ranges::all_of(
            name, [](unsigned char character) { return character >= 0x20U && character <= 0x7EU; });
    }

    inline void appendName(std::vector<std::uint8_t>& output, const std::string& name) {
        std::array<std::uint8_t, Room::MaxPlayerNameLength> bytes{};
        std::copy_n(name.begin(), std::min(name.size(), bytes.size()), bytes.begin());
        output.insert(output.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] inline bool readName(const std::uint8_t* data, std::size_t size, std::size_t& offset,
                                       std::string& name) {
        if (offset + Room::MaxPlayerNameLength > size)
            return false;
        std::size_t length = 0;
        while (length < Room::MaxPlayerNameLength && data[offset + length] != 0)
            ++length;
        for (std::size_t index = length; index < Room::MaxPlayerNameLength; ++index) {
            if (data[offset + index] != 0)
                return false;
        }
        if (length == 0)
            return false;
        name.assign(reinterpret_cast<const char*>(data + offset), length);
        offset += Room::MaxPlayerNameLength;
        return validName(name);
    }

    inline void appendShortString(std::vector<std::uint8_t>& output, const std::string& value) {
        output.push_back(static_cast<std::uint8_t>(value.size()));
        output.insert(output.end(), value.begin(), value.end());
    }

    [[nodiscard]] inline bool readShortString(const std::uint8_t* data, std::size_t size,
                                              std::size_t& offset, std::string& value) {
        if (offset >= size)
            return false;
        const auto length = static_cast<std::size_t>(data[offset++]);
        if (offset + length > size)
            return false;
        value.assign(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        return true;
    }

    [[nodiscard]] inline bool validRoomName(const std::string& name) {
        if (name.empty() || name.size() > Room::MaxRoomNameLength)
            return false;
        return std::ranges::all_of(
            name, [](unsigned char character) { return character >= 0x20U && character <= 0x7EU; });
    }

    [[nodiscard]] inline bool validChatText(const std::string& text) {
        if (text.empty() || text.size() > Chat::MaxTextLength)
            return false;
        return std::ranges::all_of(
            text, [](unsigned char character) { return character >= 0x20U && character <= 0x7EU; });
    }

    inline std::vector<std::uint8_t> makeLobbyFrame(eLobbyCommand                    command,
                                                    const std::vector<std::uint8_t>& payload) {
        std::vector<std::uint8_t> frame = {
            Magic.at(0),
            Magic.at(1),
            Magic.at(2),
            static_cast<std::uint8_t>(command),
        };
        appendU16BE(frame, static_cast<std::uint16_t>(payload.size()));
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    [[nodiscard]] inline bool readLobbyHeader(const std::uint8_t* data, std::size_t size,
                                              eLobbyCommand& command, std::uint16_t& payloadLength) {
        if (size < LobbyHeaderSize)
            return false;
        if (data[0] != Magic.at(0) || data[1] != Magic.at(1) || data[2] != Magic.at(2))
            return false;
        command       = static_cast<eLobbyCommand>(data[3]);
        payloadLength = readU16BE(data + 4);
        return payloadLength <= MaxLobbyPayload;
    }

    inline std::vector<std::uint8_t> makeJoin(const std::string& playerName) {
        if (!validName(playerName))
            return {};
        std::vector<std::uint8_t> payload;
        appendName(payload, playerName);
        return makeLobbyFrame(eLobbyCommand::JOIN, payload);
    }

    [[nodiscard]] inline bool readJoin(const std::uint8_t* payload, std::size_t size,
                                       std::string& playerName) {
        std::size_t offset = 0;
        return readName(payload, size, offset, playerName) && offset == size;
    }

    inline std::vector<std::uint8_t> makeRoomListRequest() {
        return makeLobbyFrame(eLobbyCommand::ROOM_LIST_REQUEST, {});
    }

    [[nodiscard]] inline bool readRoomListRequest(const std::uint8_t*, std::size_t size) {
        return size == 0;
    }

    inline std::vector<std::uint8_t> makeRoomList(const std::vector<Room::SSummary>& rooms) {
        std::vector<std::uint8_t> payload;
        appendU16BE(payload, static_cast<std::uint16_t>(rooms.size()));
        for (const auto& room : rooms) {
            appendU16BE(payload, room.id);
            appendShortString(payload, room.name);
            payload.push_back(room.playerCount);
            payload.push_back(room.maxPlayers);
            payload.push_back(static_cast<std::uint8_t>(room.state));
            appendName(payload, room.masterName);
        }
        return makeLobbyFrame(eLobbyCommand::ROOM_LIST, payload);
    }

    [[nodiscard]] inline bool readRoomList(const std::uint8_t* payload, std::size_t size,
                                           std::vector<Room::SSummary>& rooms) {
        if (size < 2)
            return false;
        const auto  count  = readU16BE(payload);
        std::size_t offset = 2;
        rooms.clear();
        rooms.reserve(count);
        for (std::uint16_t index = 0; index < count; ++index) {
            Room::SSummary room;
            if (offset + 2 > size)
                return false;
            room.id = readU16BE(payload + offset);
            offset += 2;
            if (!readShortString(payload, size, offset, room.name))
                return false;
            if (offset + 3 > size)
                return false;
            room.playerCount = payload[offset++];
            room.maxPlayers  = payload[offset++];
            room.state       = static_cast<Room::eState>(payload[offset++]);
            if (!readName(payload, size, offset, room.masterName))
                return false;
            rooms.push_back(std::move(room));
        }
        return offset == size;
    }

    inline std::vector<std::uint8_t> makeCreateRoom(const std::string& roomName) {
        if (!validRoomName(roomName))
            return {};
        std::vector<std::uint8_t> payload;
        appendShortString(payload, roomName);
        return makeLobbyFrame(eLobbyCommand::CREATE_ROOM, payload);
    }

    [[nodiscard]] inline bool readCreateRoom(const std::uint8_t* payload, std::size_t size,
                                             std::string& roomName) {
        std::size_t offset = 0;
        return readShortString(payload, size, offset, roomName) && offset == size &&
            validRoomName(roomName);
    }

    inline std::vector<std::uint8_t> makeSelectRoom(std::uint16_t roomId) {
        std::vector<std::uint8_t> payload;
        appendU16BE(payload, roomId);
        return makeLobbyFrame(eLobbyCommand::SELECT_ROOM, payload);
    }

    [[nodiscard]] inline bool readSelectRoom(const std::uint8_t* payload, std::size_t size,
                                             std::uint16_t& roomId) {
        if (size != 2)
            return false;
        roomId = readU16BE(payload);
        return true;
    }

    inline std::vector<std::uint8_t> makeRoomInfo(eLobbyCommand command, const Room::SInfo& room) {
        std::vector<std::uint8_t> payload;
        appendU16BE(payload, room.id);
        appendShortString(payload, room.name);
        payload.push_back(static_cast<std::uint8_t>(room.state));
        payload.push_back(room.masterId);
        payload.push_back(static_cast<std::uint8_t>(room.players.size()));
        for (const auto& player : room.players) {
            payload.push_back(player.id);
            appendName(payload, player.name);
        }
        return makeLobbyFrame(command, payload);
    }

    [[nodiscard]] inline bool readRoomInfo(const std::uint8_t* payload, std::size_t size,
                                           Room::SInfo& room) {
        std::size_t offset = 0;
        if (offset + 2 > size)
            return false;
        room.id = readU16BE(payload + offset);
        offset += 2;
        if (!readShortString(payload, size, offset, room.name))
            return false;
        if (offset + 3 > size)
            return false;
        room.state       = static_cast<Room::eState>(payload[offset++]);
        room.masterId    = payload[offset++];
        const auto count = payload[offset++];
        room.players.clear();
        room.players.reserve(count);
        for (std::uint8_t index = 0; index < count; ++index) {
            Room::SPlayer player;
            if (offset + 1 > size)
                return false;
            player.id = payload[offset++];
            if (!readName(payload, size, offset, player.name))
                return false;
            room.players.push_back(std::move(player));
        }
        return offset == size;
    }

    inline std::vector<std::uint8_t> makeRoomRejected(Room::eRejectReason reason,
                                                      const std::string&  message) {
        std::vector<std::uint8_t> payload;
        payload.push_back(static_cast<std::uint8_t>(reason));
        appendShortString(payload, message);
        return makeLobbyFrame(eLobbyCommand::ROOM_REJECTED, payload);
    }

    [[nodiscard]] inline bool readRoomRejected(const std::uint8_t* payload, std::size_t size,
                                               Room::eRejectReason& reason, std::string& message) {
        if (size < 1)
            return false;
        reason             = static_cast<Room::eRejectReason>(payload[0]);
        std::size_t offset = 1;
        return readShortString(payload, size, offset, message) && offset == size;
    }

    inline std::vector<std::uint8_t> makeLaunchGame() {
        return makeLobbyFrame(eLobbyCommand::LAUNCH_GAME, {});
    }

    [[nodiscard]] inline bool readLaunchGame(const std::uint8_t*, std::size_t size) {
        return size == 0;
    }

    inline std::vector<std::uint8_t> makeGameStarted(std::uint16_t roomId) {
        std::vector<std::uint8_t> payload;
        appendU16BE(payload, roomId);
        return makeLobbyFrame(eLobbyCommand::GAME_STARTED, payload);
    }

    [[nodiscard]] inline bool readGameStarted(const std::uint8_t* payload, std::size_t size,
                                              std::uint16_t& roomId) {
        if (size != 2)
            return false;
        roomId = readU16BE(payload);
        return true;
    }

    inline std::vector<std::uint8_t> makeLeaveRoom() {
        return makeLobbyFrame(eLobbyCommand::LEAVE_ROOM, {});
    }

    [[nodiscard]] inline bool readLeaveRoom(const std::uint8_t*, std::size_t size) {
        return size == 0;
    }

    inline std::vector<std::uint8_t> makeRoomLeft() {
        return makeLobbyFrame(eLobbyCommand::ROOM_LEFT, {});
    }

    [[nodiscard]] inline bool readRoomLeft(const std::uint8_t*, std::size_t size) {
        return size == 0;
    }

    // Chat frames carry a serialized SCChatMessage component, so they are packed and
    // unpacked by the component itself and the shared component registry.
    [[nodiscard]] inline bool readChatComponent(const std::uint8_t* payload, std::size_t size,
                                                SCChatMessage& message) {
        const CComponentRegistry registry;
        const auto  component = registry.deserialize(std::vector<std::uint8_t>(payload, payload + size));
        const auto* decoded   = dynamic_cast<const SCChatMessage*>(component.get());
        if (decoded == nullptr)
            return false;
        message = *decoded;
        return validChatText(message.text);
    }

    // Client to server: only the text of `message` is meaningful.
    inline std::vector<std::uint8_t> makeChatSend(const SCChatMessage& message) {
        if (!validChatText(message.text))
            return {};
        std::vector<std::uint8_t> payload;
        SCChatMessage{0, 0, {}, message.text}.serialize(payload);
        return makeLobbyFrame(eLobbyCommand::CHAT_SEND, payload);
    }

    [[nodiscard]] inline bool readChatSend(const std::uint8_t* payload, std::size_t size,
                                           SCChatMessage& message) {
        return readChatComponent(payload, size, message);
    }

    // Server to client: the message is stamped with its sender and sequence.
    inline std::vector<std::uint8_t> makeChatMessage(const SCChatMessage& message) {
        if (!validName(message.sender) || !validChatText(message.text))
            return {};
        std::vector<std::uint8_t> payload;
        message.serialize(payload);
        return makeLobbyFrame(eLobbyCommand::CHAT_MESSAGE, payload);
    }

    [[nodiscard]] inline bool readChatMessage(const std::uint8_t* payload, std::size_t size,
                                              SCChatMessage& message) {
        return readChatComponent(payload, size, message) && validName(message.sender);
    }

} // namespace Net
