#pragma once

#include "Shared/Wire.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Net::Control {

    inline constexpr std::uint16_t DefaultMasterPort = 7100;
    inline constexpr std::uint8_t  ProtocolMajor     = 1;
    inline constexpr std::uint8_t  ProtocolMinor     = 0;
    inline constexpr std::uint16_t MaxPayload        = 8192;

    enum class eCommand : std::uint8_t {
        REGISTER            = 11,
        KEEP_ALIVE          = 12,
        STATUS              = 13,
        REGISTER_ACK        = 14,
        UNREGISTER          = 15,
        UNREGISTER_ACK      = 16,
        SERVER_LIST_REQUEST = 20,
        SERVER_LIST         = 21,
        ERROR               = 22,
    };

    enum class eServerState : std::uint8_t {
        STARTING = 0,
        READY    = 1,
        DRAINING = 2,
        STOPPING = 3,
    };

    enum class eRegisterResult : std::uint8_t {
        ACCEPTED     = 0,
        INVALID      = 1,
        INCOMPATIBLE = 2,
        CONFLICT     = 3,
        UNAVAILABLE  = 4,
    };

    enum class eErrorCode : std::uint8_t {
        MALFORMED   = 0,
        UNSUPPORTED = 1,
        UNAVAILABLE = 2,
    };

    struct SRegister {
        std::string   name;
        std::uint16_t tcpPort    = 0;
        std::uint16_t udpPort    = 0;
        std::uint16_t maxPlayers = 0;
        std::uint8_t  major      = ProtocolMajor;
        std::uint8_t  minor      = ProtocolMinor;
        std::string   address;
    };

    struct SRegisterAck {
        eRegisterResult result = eRegisterResult::INVALID;
        std::string     message;
    };

    struct SStatus {
        std::uint16_t rooms            = 0;
        std::uint16_t connectedPlayers = 0;
        std::uint16_t maxPlayers       = 0;
        std::uint16_t joinableRooms    = 0;
        std::uint8_t  sequence         = 0;
        eServerState  state            = eServerState::STARTING;
    };

    struct SServerEntry {
        std::string   name;
        std::uint16_t tcpPort          = 0;
        std::uint16_t udpPort          = 0;
        std::uint16_t connectedPlayers = 0;
        std::uint16_t maxPlayers       = 0;
        std::uint16_t rooms            = 0;
        std::uint16_t joinableRooms    = 0;
        std::uint8_t  major            = ProtocolMajor;
        std::uint8_t  minor            = ProtocolMinor;
        eServerState  state            = eServerState::STARTING;
        std::string   address;
    };

    struct SServerList {
        std::vector<SServerEntry> entries;
    };

    struct SUnregisterAck {
        std::uint8_t result = 0;
        std::string  message;
    };

    struct SError {
        eErrorCode  code = eErrorCode::MALFORMED;
        std::string message;
    };

    inline std::vector<std::uint8_t> makeFrame(eCommand                         command,
                                               const std::vector<std::uint8_t>& payload) {
        return Net::makeFrame(static_cast<std::uint8_t>(command), payload);
    }

    [[nodiscard]] inline bool readHeader(const std::uint8_t* data, std::size_t size, eCommand& command,
                                         std::uint16_t& payloadLength) {
        std::uint8_t rawCommand = 0;
        if (!Net::readFrameHeader(data, size, rawCommand, payloadLength, MaxPayload))
            return false;
        command = static_cast<eCommand>(rawCommand);
        return true;
    }

    inline std::vector<std::uint8_t> makeRegister(const SRegister& registration) {
        if (!Net::validName(registration.name) || registration.tcpPort == 0 ||
            registration.udpPort == 0 || registration.maxPlayers == 0 || registration.address.empty())
            return {};
        std::vector<std::uint8_t> payload;
        Net::appendName(payload, registration.name);
        Net::appendU16BE(payload, registration.tcpPort);
        Net::appendU16BE(payload, registration.udpPort);
        Net::appendU16BE(payload, registration.maxPlayers);
        payload.push_back(registration.major);
        payload.push_back(registration.minor);
        Net::appendShortString(payload, registration.address);
        return makeFrame(eCommand::REGISTER, payload);
    }

    [[nodiscard]] inline bool readRegister(const std::uint8_t* payload, std::size_t size,
                                           SRegister& registration) {
        std::size_t offset = 0;
        if (!Net::readName(payload, size, offset, registration.name))
            return false;
        if (offset + 8 > size)
            return false;
        registration.tcpPort = Net::readU16BE(payload + offset);
        offset += 2;
        registration.udpPort = Net::readU16BE(payload + offset);
        offset += 2;
        registration.maxPlayers = Net::readU16BE(payload + offset);
        offset += 2;
        registration.major = payload[offset++];
        registration.minor = payload[offset++];
        if (!Net::readShortString(payload, size, offset, registration.address))
            return false;
        return offset == size && registration.tcpPort != 0 && registration.udpPort != 0 &&
            registration.maxPlayers != 0 && !registration.address.empty();
    }

    inline std::vector<std::uint8_t> makeRegisterAck(const SRegisterAck& ack) {
        std::vector<std::uint8_t> payload;
        payload.push_back(static_cast<std::uint8_t>(ack.result));
        Net::appendShortString(payload, ack.message);
        return makeFrame(eCommand::REGISTER_ACK, payload);
    }

    [[nodiscard]] inline bool readRegisterAck(const std::uint8_t* payload, std::size_t size,
                                              SRegisterAck& ack) {
        if (size < 1)
            return false;
        ack.result         = static_cast<eRegisterResult>(payload[0]);
        std::size_t offset = 1;
        return Net::readShortString(payload, size, offset, ack.message) && offset == size;
    }

    inline std::vector<std::uint8_t> makeKeepAlive(std::uint8_t sequence) {
        return makeFrame(eCommand::KEEP_ALIVE, {sequence});
    }

    [[nodiscard]] inline bool readKeepAlive(const std::uint8_t* payload, std::size_t size,
                                            std::uint8_t& sequence) {
        if (size != 1)
            return false;
        sequence = payload[0];
        return true;
    }

    inline std::vector<std::uint8_t> makeStatus(const SStatus& status) {
        std::vector<std::uint8_t> payload;
        Net::appendU16BE(payload, status.rooms);
        Net::appendU16BE(payload, status.connectedPlayers);
        Net::appendU16BE(payload, status.maxPlayers);
        Net::appendU16BE(payload, status.joinableRooms);
        payload.push_back(status.sequence);
        payload.push_back(static_cast<std::uint8_t>(status.state));
        return makeFrame(eCommand::STATUS, payload);
    }

    [[nodiscard]] inline bool readStatus(const std::uint8_t* payload, std::size_t size,
                                         SStatus& status) {
        if (size != 10)
            return false;
        status.rooms            = Net::readU16BE(payload);
        status.connectedPlayers = Net::readU16BE(payload + 2);
        status.maxPlayers       = Net::readU16BE(payload + 4);
        status.joinableRooms    = Net::readU16BE(payload + 6);
        status.sequence         = payload[8];
        status.state            = static_cast<eServerState>(payload[9]);
        return true;
    }

    inline std::vector<std::uint8_t> makeUnregister() {
        return makeFrame(eCommand::UNREGISTER, {});
    }

    [[nodiscard]] inline bool readUnregister(const std::uint8_t*, std::size_t size) {
        return size == 0;
    }

    inline std::vector<std::uint8_t> makeUnregisterAck(const SUnregisterAck& ack) {
        std::vector<std::uint8_t> payload;
        payload.push_back(ack.result);
        Net::appendShortString(payload, ack.message);
        return makeFrame(eCommand::UNREGISTER_ACK, payload);
    }

    [[nodiscard]] inline bool readUnregisterAck(const std::uint8_t* payload, std::size_t size,
                                                SUnregisterAck& ack) {
        if (size < 1)
            return false;
        ack.result         = payload[0];
        std::size_t offset = 1;
        return Net::readShortString(payload, size, offset, ack.message) && offset == size;
    }

    inline std::vector<std::uint8_t> makeServerListRequest(std::uint8_t major, std::uint8_t minor) {
        return makeFrame(eCommand::SERVER_LIST_REQUEST, {major, minor});
    }

    [[nodiscard]] inline bool readServerListRequest(const std::uint8_t* payload, std::size_t size,
                                                    std::uint8_t& major, std::uint8_t& minor) {
        if (size != 2)
            return false;
        major = payload[0];
        minor = payload[1];
        return true;
    }

    inline std::vector<std::uint8_t> makeServerList(const SServerList& list) {
        std::vector<std::uint8_t> payload;
        Net::appendU16BE(payload, static_cast<std::uint16_t>(list.entries.size()));
        for (const auto& entry : list.entries) {
            Net::appendName(payload, entry.name);
            Net::appendU16BE(payload, entry.tcpPort);
            Net::appendU16BE(payload, entry.udpPort);
            Net::appendU16BE(payload, entry.connectedPlayers);
            Net::appendU16BE(payload, entry.maxPlayers);
            Net::appendU16BE(payload, entry.rooms);
            Net::appendU16BE(payload, entry.joinableRooms);
            payload.push_back(entry.major);
            payload.push_back(entry.minor);
            payload.push_back(static_cast<std::uint8_t>(entry.state));
            Net::appendShortString(payload, entry.address);
        }
        return makeFrame(eCommand::SERVER_LIST, payload);
    }

    [[nodiscard]] inline bool readServerList(const std::uint8_t* payload, std::size_t size,
                                             SServerList& list) {
        if (size < 2)
            return false;
        const auto  count  = Net::readU16BE(payload);
        std::size_t offset = 2;
        list.entries.clear();
        list.entries.reserve(count);
        for (std::uint16_t index = 0; index < count; ++index) {
            SServerEntry entry;
            if (!Net::readName(payload, size, offset, entry.name))
                return false;
            if (offset + 15 > size)
                return false;
            entry.tcpPort = Net::readU16BE(payload + offset);
            offset += 2;
            entry.udpPort = Net::readU16BE(payload + offset);
            offset += 2;
            entry.connectedPlayers = Net::readU16BE(payload + offset);
            offset += 2;
            entry.maxPlayers = Net::readU16BE(payload + offset);
            offset += 2;
            entry.rooms = Net::readU16BE(payload + offset);
            offset += 2;
            entry.joinableRooms = Net::readU16BE(payload + offset);
            offset += 2;
            entry.major = payload[offset++];
            entry.minor = payload[offset++];
            entry.state = static_cast<eServerState>(payload[offset++]);
            if (!Net::readShortString(payload, size, offset, entry.address))
                return false;
            list.entries.push_back(std::move(entry));
        }
        return offset == size;
    }

    inline std::vector<std::uint8_t> makeError(eErrorCode code, const std::string& message) {
        std::vector<std::uint8_t> payload;
        payload.push_back(static_cast<std::uint8_t>(code));
        Net::appendShortString(payload, message);
        return makeFrame(eCommand::ERROR, payload);
    }

    [[nodiscard]] inline bool readError(const std::uint8_t* payload, std::size_t size, SError& error) {
        if (size < 1)
            return false;
        error.code         = static_cast<eErrorCode>(payload[0]);
        std::size_t offset = 1;
        return Net::readShortString(payload, size, offset, error.message) && offset == size;
    }

} // namespace Net::Control
