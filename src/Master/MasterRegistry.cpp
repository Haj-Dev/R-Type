#include "MasterRegistry.hpp"

#include <utility>

namespace {

    [[nodiscard]] bool validAdvertisedAddress(const std::string& address) {
        return !address.empty() && address != "0.0.0.0" && address != "::" && address != "*";
    }

    [[nodiscard]] bool validServerState(Net::Control::eServerState state) {
        return state == Net::Control::eServerState::STARTING ||
            state == Net::Control::eServerState::READY ||
            state == Net::Control::eServerState::DRAINING ||
            state == Net::Control::eServerState::STOPPING;
    }

} // namespace

Net::Control::eRegisterResult
Master::CMasterRegistry::registerServer(ConnectionId id, const Net::Control::SRegister& request,
                                        std::string& message) {
    if (!Net::validName(request.name)) {
        message = "Invalid server name";
        return Net::Control::eRegisterResult::INVALID;
    }
    if (request.tcpPort == 0 || request.udpPort == 0 || request.maxPlayers == 0) {
        message = "Ports and max players must be non-zero";
        return Net::Control::eRegisterResult::INVALID;
    }
    if (!validAdvertisedAddress(request.address)) {
        message = "Advertised address is not usable";
        return Net::Control::eRegisterResult::INVALID;
    }
    if (request.major != Net::Control::ProtocolMajor) {
        message = "Unsupported protocol version";
        return Net::Control::eRegisterResult::INCOMPATIBLE;
    }

    const auto existing = mRegistrations.find(id);
    if (existing != mRegistrations.end() && existing->second.info.name != request.name) {
        message = "Unregister before renaming an active registration";
        return Net::Control::eRegisterResult::INVALID;
    }

    const auto nameOwner = mNames.find(request.name);
    if (nameOwner != mNames.end() && nameOwner->second != id) {
        message = "Another live connection owns this name";
        return Net::Control::eRegisterResult::CONFLICT;
    }

    SRegistration registration;
    registration.info      = request;
    registration.healthy   = true;
    registration.lastSeen  = std::chrono::steady_clock::now();
    registration.lastProbe = registration.lastSeen;
    if (existing != mRegistrations.end()) {
        registration.rooms            = existing->second.rooms;
        registration.connectedPlayers = existing->second.connectedPlayers;
        registration.joinableRooms    = existing->second.joinableRooms;
        registration.state            = existing->second.state;
        registration.nextSequence     = existing->second.nextSequence;
    }
    mRegistrations.insert_or_assign(id, std::move(registration));
    mNames.insert_or_assign(request.name, id);
    message = "Registration accepted";
    return Net::Control::eRegisterResult::ACCEPTED;
}

void Master::CMasterRegistry::unregisterServer(ConnectionId id) {
    const auto registration = mRegistrations.find(id);
    if (registration == mRegistrations.end())
        return;
    mNames.erase(registration->second.info.name);
    mRegistrations.erase(registration);
}

bool Master::CMasterRegistry::updateStatus(ConnectionId id, const Net::Control::SStatus& status,
                                           std::string& message) {
    const auto registration = mRegistrations.find(id);
    if (registration == mRegistrations.end()) {
        message = "No active registration";
        return false;
    }
    if (!registration->second.outstandingSequence) {
        message = "No outstanding health check";
        return false;
    }
    if (status.sequence != *registration->second.outstandingSequence) {
        message = "Status sequence does not match the outstanding probe";
        return false;
    }
    if (status.connectedPlayers > status.maxPlayers) {
        message = "Connected players exceed max players";
        return false;
    }
    if (status.joinableRooms > status.rooms) {
        message = "Joinable rooms exceed rooms";
        return false;
    }
    if (status.maxPlayers != registration->second.info.maxPlayers) {
        message = "Max players does not match the registration";
        return false;
    }
    if (!validServerState(status.state)) {
        message = "Unknown server state";
        return false;
    }

    registration->second.rooms            = status.rooms;
    registration->second.connectedPlayers = status.connectedPlayers;
    registration->second.joinableRooms    = status.joinableRooms;
    registration->second.state            = status.state;
    registration->second.healthy          = true;
    registration->second.lastSeen         = std::chrono::steady_clock::now();
    registration->second.outstandingSequence.reset();
    message = "Status accepted";
    return true;
}

std::vector<Net::Control::SServerEntry> Master::CMasterRegistry::listServers(std::uint8_t major,
                                                                             std::uint8_t minor) const {
    std::vector<Net::Control::SServerEntry> entries;
    entries.reserve(mRegistrations.size());
    for (const auto& [id, registration] : mRegistrations) {
        static_cast<void>(id);
        if (!registration.healthy || registration.state != Net::Control::eServerState::READY)
            continue;
        if (registration.connectedPlayers >= registration.info.maxPlayers)
            continue;
        if (registration.info.major != major || registration.info.minor > minor)
            continue;
        entries.push_back(Net::Control::SServerEntry{
            .name             = registration.info.name,
            .tcpPort          = registration.info.tcpPort,
            .udpPort          = registration.info.udpPort,
            .connectedPlayers = registration.connectedPlayers,
            .maxPlayers       = registration.info.maxPlayers,
            .rooms            = registration.rooms,
            .joinableRooms    = registration.joinableRooms,
            .major            = registration.info.major,
            .minor            = registration.info.minor,
            .state            = registration.state,
            .address          = registration.info.address,
        });
    }
    return entries;
}

std::optional<std::uint8_t> Master::CMasterRegistry::beginHealthCheck(ConnectionId id) {
    const auto registration = mRegistrations.find(id);
    if (registration == mRegistrations.end())
        return std::nullopt;
    if (registration->second.outstandingSequence)
        return std::nullopt;
    const auto sequence                      = registration->second.nextSequence++;
    registration->second.outstandingSequence = sequence;
    registration->second.lastProbe           = std::chrono::steady_clock::now();
    return sequence;
}

bool Master::CMasterRegistry::checkHealth(ConnectionId id, std::chrono::steady_clock::time_point now,
                                          std::chrono::milliseconds responseDeadline,
                                          std::chrono::milliseconds unhealthyTimeout) {
    const auto registration = mRegistrations.find(id);
    if (registration == mRegistrations.end())
        return true;
    if (registration->second.outstandingSequence &&
        now - registration->second.lastProbe > responseDeadline)
        return false;
    if (now - registration->second.lastSeen > unhealthyTimeout)
        registration->second.healthy = false;
    return true;
}

void Master::CMasterRegistry::removeConnection(ConnectionId id) {
    unregisterServer(id);
}

bool Master::CMasterRegistry::hasRegistration(ConnectionId id) const {
    return mRegistrations.contains(id);
}

std::optional<Net::Control::SRegister> Master::CMasterRegistry::registration(ConnectionId id) const {
    const auto registration = mRegistrations.find(id);
    if (registration == mRegistrations.end())
        return std::nullopt;
    return registration->second.info;
}

std::size_t Master::CMasterRegistry::size() const {
    return mRegistrations.size();
}
