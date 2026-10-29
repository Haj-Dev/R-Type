#pragma once

#include "Shared/Lobby.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Owns the lobby-side room state. Player identifiers are the connection IDs
// assigned by the TCP handshake (0..Room::MaxConnections-1). Each room is
// capped at Room::MaxPlayersPerRoom players.
class CRoomManager {
  public:
    [[nodiscard]] Room::SResult setPlayerName(std::uint8_t playerId, const std::string& name);
    [[nodiscard]] Room::SResult createRoom(std::uint8_t playerId, const std::string& roomName);
    [[nodiscard]] Room::SResult joinRoom(std::uint8_t playerId, std::uint16_t roomId);
    [[nodiscard]] Room::SResult leaveRoom(std::uint8_t playerId);
    [[nodiscard]] Room::SResult launch(std::uint8_t playerId);
    void                        removePlayer(std::uint8_t playerId);

    [[nodiscard]] std::vector<Room::SSummary>  listRooms() const;
    [[nodiscard]] std::optional<Room::SInfo>   roomInfo(std::uint16_t roomId) const;
    [[nodiscard]] std::optional<Room::SInfo>   roomOfPlayer(std::uint8_t playerId) const;
    [[nodiscard]] std::optional<std::uint16_t> roomIdOfPlayer(std::uint8_t playerId) const;
    [[nodiscard]] std::vector<std::uint8_t>    playersInRoom(std::uint16_t roomId) const;
    [[nodiscard]] bool                         isMaster(std::uint8_t playerId) const;
    [[nodiscard]] std::optional<std::uint16_t> activeRoom() const;
    [[nodiscard]] std::uint16_t                roomCount() const;
    [[nodiscard]] std::uint16_t                joinableRoomCount() const;
    [[nodiscard]] std::uint16_t                connectedPlayerCount() const;
    [[nodiscard]] std::string                  playerName(std::uint8_t playerId) const;
    [[nodiscard]] bool                         hasPlayer(std::uint8_t playerId) const;

  private:
    struct SRoom {
        std::uint16_t             id = 0;
        std::string               name;
        Room::eState              state    = Room::eState::WAITING;
        std::uint8_t              masterId = 0;
        std::vector<std::uint8_t> players;
    };

    [[nodiscard]] SRoom*                                           findRoom(std::uint16_t roomId);
    [[nodiscard]] const SRoom*                                     findRoom(std::uint16_t roomId) const;
    [[nodiscard]] Room::SInfo                                      makeInfo(const SRoom& room) const;
    void                                                           removeRoom(std::uint16_t roomId);

    std::array<std::string, Room::MaxConnections>                  mPlayerNames{};
    std::array<std::optional<std::uint16_t>, Room::MaxConnections> mPlayerRooms{};
    std::vector<SRoom>                                             mRooms;
    std::uint16_t                                                  mNextRoomId = 1;
    std::optional<std::uint16_t>                                   mActiveRoom;
};
