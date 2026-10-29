#pragma once

#include "Shared/ControlProtocol.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Master {

    using ConnectionId = std::uint64_t;

    struct SRegistration {
        Net::Control::SRegister               info;
        std::uint16_t                         rooms            = 0;
        std::uint16_t                         connectedPlayers = 0;
        std::uint16_t                         joinableRooms    = 0;
        Net::Control::eServerState            state            = Net::Control::eServerState::STARTING;
        bool                                  healthy          = false;
        std::chrono::steady_clock::time_point lastSeen;
        std::optional<std::uint8_t>           outstandingSequence;
        std::chrono::steady_clock::time_point lastProbe;
        std::uint8_t                          nextSequence = 0;
    };

    class CMasterRegistry {
      public:
        [[nodiscard]] Net::Control::eRegisterResult
        registerServer(ConnectionId id, const Net::Control::SRegister& request, std::string& message);
        void               unregisterServer(ConnectionId id);
        [[nodiscard]] bool updateStatus(ConnectionId id, const Net::Control::SStatus& status,
                                        std::string& message);
        [[nodiscard]] std::vector<Net::Control::SServerEntry> listServers(std::uint8_t major,
                                                                          std::uint8_t minor) const;
        [[nodiscard]] std::optional<std::uint8_t>             beginHealthCheck(ConnectionId id);
        [[nodiscard]] bool checkHealth(ConnectionId id, std::chrono::steady_clock::time_point now,
                                       std::chrono::milliseconds responseDeadline,
                                       std::chrono::milliseconds unhealthyTimeout);
        void               removeConnection(ConnectionId id);

        [[nodiscard]] bool hasRegistration(ConnectionId id) const;
        [[nodiscard]] std::optional<Net::Control::SRegister> registration(ConnectionId id) const;
        [[nodiscard]] std::size_t                            size() const;

      private:
        std::unordered_map<ConnectionId, SRegistration> mRegistrations;
        std::unordered_map<std::string, ConnectionId>   mNames;
    };

} // namespace Master
