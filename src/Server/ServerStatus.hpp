#pragma once

#include "Shared/ControlProtocol.hpp"
#include "Shared/Lobby.hpp"

#include <cstdint>

struct SServerStatus {
    std::uint16_t              rooms            = 0;
    std::uint16_t              connectedPlayers = 0;
    std::uint16_t              maxPlayers       = Room::MaxConnections;
    std::uint16_t              joinableRooms    = 0;
    Net::Control::eServerState state            = Net::Control::eServerState::READY;
};
