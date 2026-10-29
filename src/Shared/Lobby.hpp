#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Room {

    inline constexpr std::uint8_t  MaxPlayersPerRoom   = 4;
    inline constexpr std::size_t   MaxConnections      = 64;
    inline constexpr std::uint16_t MaxRooms            = 64;
    inline constexpr std::size_t   MaxPlayerNameLength = 8;
    inline constexpr std::size_t   MaxRoomNameLength   = 24;

    enum class eState : std::uint8_t {
        WAITING = 0,
        IN_GAME = 1,
    };

    enum class eRejectReason : std::uint8_t {
        INVALID_NAME         = 0,
        INVALID_ROOM_NAME    = 1,
        ROOM_NOT_FOUND       = 2,
        ROOM_FULL            = 3,
        ROOM_IN_GAME         = 4,
        ALREADY_IN_ROOM      = 5,
        NOT_IN_ROOM          = 6,
        NOT_MASTER           = 7,
        GAME_ALREADY_RUNNING = 8,
        SERVER_ERROR         = 9,
    };

    struct SResult {
        bool          ok     = false;
        eRejectReason reason = eRejectReason::SERVER_ERROR;
        std::string   message;
    };

    struct SPlayer {
        std::uint8_t id = 0;
        std::string  name;
    };

    struct SSummary {
        std::uint16_t id = 0;
        std::string   name;
        std::uint8_t  playerCount = 0;
        std::uint8_t  maxPlayers  = MaxPlayersPerRoom;
        eState        state       = eState::WAITING;
        std::string   masterName;
    };

    struct SInfo {
        std::uint16_t        id = 0;
        std::string          name;
        eState               state    = eState::WAITING;
        std::uint8_t         masterId = 0;
        std::vector<SPlayer> players;
    };

    inline SResult success() {
        return SResult{.ok = true, .reason = eRejectReason::SERVER_ERROR, .message = {}};
    }

    inline SResult failure(eRejectReason reason, std::string message) {
        return SResult{.ok = false, .reason = reason, .message = std::move(message)};
    }

} // namespace Room

namespace Lobby {

    enum class eLobbyPhase : std::uint8_t {
        CONNECTING = 0,
        SERVER_SELECT,
        NAME_ENTRY,
        ROOM_LIST,
        IN_ROOM,
        IN_GAME,
        ERROR,
    };

    enum class eClientCommand : std::uint8_t {
        JOIN = 0,
        CREATE_ROOM,
        SELECT_ROOM,
        LAUNCH_GAME,
        LEAVE_ROOM,
        REFRESH_ROOMS,
        SELECT_SERVER,
        REFRESH_SERVERS,
    };

    struct SCommand {
        eClientCommand type = eClientCommand::JOIN;
        std::string    text;
        std::uint16_t  roomId = 0;
    };

    struct SServerInfo {
        std::string   name;
        std::string   address;
        std::uint16_t tcpPort          = 0;
        std::uint16_t udpPort          = 0;
        std::uint16_t connectedPlayers = 0;
        std::uint16_t maxPlayers       = 0;
        std::uint16_t rooms            = 0;
        std::uint16_t joinableRooms    = 0;
        std::uint8_t  major            = 0;
        std::uint8_t  minor            = 0;
        std::uint8_t  state            = 0;
    };

    struct SLobbyState {
        eLobbyPhase                 phase    = eLobbyPhase::CONNECTING;
        std::uint8_t                playerId = 0;
        std::string                 playerName;
        std::vector<SServerInfo>    servers;
        std::vector<Room::SSummary> rooms;
        std::optional<Room::SInfo>  currentRoom;
        std::string                 message;
    };

} // namespace Lobby
