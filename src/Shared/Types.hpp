#pragma once

#include <cstdint>

namespace Net {

    inline constexpr uint16_t DefaultTcpPort = 7000;
    inline constexpr uint16_t DefaultUdpPort = 7001;

    // Packet types
    enum class ePacketType : uint8_t {
        PKT_HANDSHAKE_REQ,
        PKT_HANDSHAKE_RES,
        PKT_STATE_UPDATE,
        PKT_STATE_BROADCAST,
        PKT_PING,
        PKT_PONG,
    };

    // ── Handshake (TCP) ────────────────────────────────────────────────────────

    struct SHandshakeRequest {
        uint32_t magic; // 0x48414A00 = "HAJ"
        uint8_t  version;
        char     name[26];
    };
    // sizeof = 32

    struct SHandshakeResponse {
        uint16_t magic;
        uint8_t  accepted;
        uint8_t  _pad[3];
        int32_t  playerId;
    };
    // sizeof = 12 (2 bytes padding before playerId)

    // ── Game state (UDP) ──────────────────────────────────────────────────────

    struct STateUpdate {
        int32_t playerId;
        float   x;
        float   y;
    };
    // sizeof = 12

    // ── Helpers ────────────────────────────────────────────────────────────────

    inline ePacketType readPacketType(const void* data) {
        return static_cast<ePacketType>(static_cast<const uint8_t*>(data)[0]);
    }

} // namespace net
