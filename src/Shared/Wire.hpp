#pragma once

#include "Shared/Types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Net {

    // Shared framing for the control plane and the lobby: magic, command, u16 length.
    inline constexpr std::size_t WireHeaderSize = 6;

    inline void                  appendU16BE(std::vector<std::uint8_t>& output, std::uint16_t value) {
        output.push_back(static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 8U) & 0xFFU));
        output.push_back(static_cast<std::uint8_t>(static_cast<unsigned>(value) & 0xFFU));
    }

    [[nodiscard]] inline std::uint16_t readU16BE(const std::uint8_t* data) {
        return static_cast<std::uint16_t>((static_cast<unsigned>(data[0]) << 8U) |
                                          static_cast<unsigned>(data[1]));
    }

    [[nodiscard]] inline bool validName(const std::string& name) {
        if (name.empty() || name.size() > 8)
            return false;
        return std::ranges::all_of(
            name, [](unsigned char character) { return character >= 0x20U && character <= 0x7EU; });
    }

    inline void appendName(std::vector<std::uint8_t>& output, const std::string& name) {
        std::array<std::uint8_t, 8> bytes{};
        std::copy_n(name.begin(), std::min(name.size(), bytes.size()), bytes.begin());
        output.insert(output.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] inline bool readName(const std::uint8_t* data, std::size_t size, std::size_t& offset,
                                       std::string& name) {
        if (offset + 8 > size)
            return false;
        std::size_t length = 0;
        while (length < 8 && data[offset + length] != 0)
            ++length;
        for (std::size_t index = length; index < 8; ++index) {
            if (data[offset + index] != 0)
                return false;
        }
        if (length == 0)
            return false;
        name.assign(reinterpret_cast<const char*>(data + offset), length);
        offset += 8;
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

    inline std::vector<std::uint8_t> makeFrame(std::uint8_t                     command,
                                               const std::vector<std::uint8_t>& payload) {
        std::vector<std::uint8_t> frame = {Magic.at(0), Magic.at(1), Magic.at(2), command};
        appendU16BE(frame, static_cast<std::uint16_t>(payload.size()));
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    [[nodiscard]] inline bool readFrameHeader(const std::uint8_t* data, std::size_t size,
                                              std::uint8_t& command, std::uint16_t& payloadLength,
                                              std::uint16_t maxPayload) {
        if (size < WireHeaderSize)
            return false;
        if (data[0] != Magic.at(0) || data[1] != Magic.at(1) || data[2] != Magic.at(2))
            return false;
        command       = data[3];
        payloadLength = readU16BE(data + 4);
        return payloadLength <= maxPayload;
    }

} // namespace Net
