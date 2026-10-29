#include "RoomManager.hpp"

#include "Shared/LobbyProtocol.hpp"

#include <algorithm>
#include <utility>

namespace {

    [[nodiscard]] bool validPlayerId(std::uint8_t playerId) {
        return playerId < Room::MaxConnections;
    }

} // namespace

Room::SResult CRoomManager::setPlayerName(std::uint8_t playerId, const std::string& name) {
    if (!validPlayerId(playerId))
        return Room::failure(Room::eRejectReason::INVALID_NAME, "Invalid player id");
    if (!Net::validName(name))
        return Room::failure(Room::eRejectReason::INVALID_NAME, "Name must be 1-8 printable characters");
    if (mPlayerRooms.at(playerId))
        return Room::failure(Room::eRejectReason::ALREADY_IN_ROOM,
                             "Leave your room before changing your name");
    mPlayerNames.at(playerId) = name;
    return Room::success();
}

Room::SResult CRoomManager::createRoom(std::uint8_t playerId, const std::string& roomName) {
    if (!validPlayerId(playerId) || !hasPlayer(playerId))
        return Room::failure(Room::eRejectReason::INVALID_NAME, "Join the lobby first");
    if (mPlayerRooms.at(playerId))
        return Room::failure(Room::eRejectReason::ALREADY_IN_ROOM, "You are already in a room");
    if (!Net::validRoomName(roomName))
        return Room::failure(Room::eRejectReason::INVALID_ROOM_NAME,
                             "Room name must be 1-24 printable characters");
    if (mRooms.size() >= Room::MaxRooms)
        return Room::failure(Room::eRejectReason::SERVER_ERROR, "The server has reached its room limit");

    SRoom room;
    room.id = mNextRoomId++;
    if (mNextRoomId == 0)
        mNextRoomId = 1;
    room.name     = roomName;
    room.state    = Room::eState::WAITING;
    room.masterId = playerId;
    room.players.push_back(playerId);
    mRooms.push_back(std::move(room));
    mPlayerRooms.at(playerId) = mRooms.back().id;
    return Room::success();
}

Room::SResult CRoomManager::joinRoom(std::uint8_t playerId, std::uint16_t roomId) {
    if (!validPlayerId(playerId) || !hasPlayer(playerId))
        return Room::failure(Room::eRejectReason::INVALID_NAME, "Join the lobby first");
    if (mPlayerRooms.at(playerId))
        return Room::failure(Room::eRejectReason::ALREADY_IN_ROOM, "You are already in a room");
    auto* room = findRoom(roomId);
    if (room == nullptr)
        return Room::failure(Room::eRejectReason::ROOM_NOT_FOUND, "Room not found");
    if (room->state != Room::eState::WAITING)
        return Room::failure(Room::eRejectReason::ROOM_IN_GAME, "That room is already in game");
    if (room->players.size() >= Room::MaxPlayersPerRoom)
        return Room::failure(Room::eRejectReason::ROOM_FULL, "That room is full");
    room->players.push_back(playerId);
    mPlayerRooms.at(playerId) = roomId;
    return Room::success();
}

Room::SResult CRoomManager::leaveRoom(std::uint8_t playerId) {
    if (!validPlayerId(playerId))
        return Room::failure(Room::eRejectReason::NOT_IN_ROOM, "Invalid player id");
    const auto roomId = mPlayerRooms.at(playerId);
    if (!roomId)
        return Room::failure(Room::eRejectReason::NOT_IN_ROOM, "You are not in a room");
    auto* room = findRoom(*roomId);
    mPlayerRooms.at(playerId).reset();
    if (room == nullptr)
        return Room::success();

    std::erase(room->players, playerId);
    if (room->players.empty()) {
        removeRoom(room->id);
        return Room::success();
    }
    if (room->masterId == playerId)
        room->masterId = room->players.front();
    return Room::success();
}

Room::SResult CRoomManager::launch(std::uint8_t playerId) {
    if (!validPlayerId(playerId))
        return Room::failure(Room::eRejectReason::NOT_IN_ROOM, "Invalid player id");
    const auto roomId = mPlayerRooms.at(playerId);
    if (!roomId)
        return Room::failure(Room::eRejectReason::NOT_IN_ROOM, "You are not in a room");
    auto* room = findRoom(*roomId);
    if (room == nullptr)
        return Room::failure(Room::eRejectReason::ROOM_NOT_FOUND, "Room not found");
    if (room->masterId != playerId)
        return Room::failure(Room::eRejectReason::NOT_MASTER,
                             "Only the room leader can launch the game");
    if (room->state != Room::eState::WAITING)
        return Room::failure(Room::eRejectReason::ROOM_IN_GAME, "The game is already running");
    if (room->players.empty())
        return Room::failure(Room::eRejectReason::SERVER_ERROR, "The room is empty");
    if (mActiveRoom && *mActiveRoom != room->id)
        return Room::failure(Room::eRejectReason::GAME_ALREADY_RUNNING,
                             "Another game is already running");
    room->state = Room::eState::IN_GAME;
    mActiveRoom = room->id;
    return Room::success();
}

void CRoomManager::removePlayer(std::uint8_t playerId) {
    if (!validPlayerId(playerId))
        return;
    if (mPlayerRooms.at(playerId))
        static_cast<void>(leaveRoom(playerId));
    mPlayerNames.at(playerId).clear();
}

std::vector<Room::SSummary> CRoomManager::listRooms() const {
    std::vector<Room::SSummary> rooms;
    rooms.reserve(mRooms.size());
    for (const auto& room : mRooms) {
        rooms.push_back(Room::SSummary{
            .id          = room.id,
            .name        = room.name,
            .playerCount = static_cast<std::uint8_t>(room.players.size()),
            .maxPlayers  = Room::MaxPlayersPerRoom,
            .state       = room.state,
            .masterName  = mPlayerNames.at(room.masterId),
        });
    }
    return rooms;
}

std::optional<Room::SInfo> CRoomManager::roomInfo(std::uint16_t roomId) const {
    const auto* room = findRoom(roomId);
    if (room == nullptr)
        return std::nullopt;
    return makeInfo(*room);
}

std::optional<Room::SInfo> CRoomManager::roomOfPlayer(std::uint8_t playerId) const {
    const auto roomId = roomIdOfPlayer(playerId);
    if (!roomId)
        return std::nullopt;
    return roomInfo(*roomId);
}

std::optional<std::uint16_t> CRoomManager::roomIdOfPlayer(std::uint8_t playerId) const {
    if (!validPlayerId(playerId))
        return std::nullopt;
    return mPlayerRooms.at(playerId);
}

std::vector<std::uint8_t> CRoomManager::playersInRoom(std::uint16_t roomId) const {
    const auto* room = findRoom(roomId);
    if (room == nullptr)
        return {};
    return room->players;
}

bool CRoomManager::isMaster(std::uint8_t playerId) const {
    const auto roomId = roomIdOfPlayer(playerId);
    if (!roomId)
        return false;
    const auto* room = findRoom(*roomId);
    return room != nullptr && room->masterId == playerId;
}

std::optional<std::uint16_t> CRoomManager::activeRoom() const {
    return mActiveRoom;
}

std::uint16_t CRoomManager::roomCount() const {
    return static_cast<std::uint16_t>(mRooms.size());
}

std::uint16_t CRoomManager::joinableRoomCount() const {
    return static_cast<std::uint16_t>(std::count_if(mRooms.begin(), mRooms.end(), [](const SRoom& room) {
        return room.state == Room::eState::WAITING && room.players.size() < Room::MaxPlayersPerRoom;
    }));
}

std::uint16_t CRoomManager::connectedPlayerCount() const {
    return static_cast<std::uint16_t>(
        std::count_if(mPlayerNames.begin(), mPlayerNames.end(),
                      [](const std::string& name) { return !name.empty(); }));
}

std::string CRoomManager::playerName(std::uint8_t playerId) const {
    if (!validPlayerId(playerId))
        return {};
    return mPlayerNames.at(playerId);
}

bool CRoomManager::hasPlayer(std::uint8_t playerId) const {
    return validPlayerId(playerId) && !mPlayerNames.at(playerId).empty();
}

CRoomManager::SRoom* CRoomManager::findRoom(std::uint16_t roomId) {
    const auto room = std::ranges::find_if(
        mRooms, [roomId](const SRoom& candidate) { return candidate.id == roomId; });
    return room == mRooms.end() ? nullptr : &*room;
}

const CRoomManager::SRoom* CRoomManager::findRoom(std::uint16_t roomId) const {
    const auto room = std::ranges::find_if(
        mRooms, [roomId](const SRoom& candidate) { return candidate.id == roomId; });
    return room == mRooms.end() ? nullptr : &*room;
}

Room::SInfo CRoomManager::makeInfo(const SRoom& room) const {
    Room::SInfo info;
    info.id       = room.id;
    info.name     = room.name;
    info.state    = room.state;
    info.masterId = room.masterId;
    info.players.reserve(room.players.size());
    for (const auto playerId : room.players) {
        info.players.push_back(Room::SPlayer{
            .id   = playerId,
            .name = mPlayerNames.at(playerId),
        });
    }
    return info;
}

void CRoomManager::removeRoom(std::uint16_t roomId) {
    std::erase_if(mRooms, [roomId](const SRoom& room) { return room.id == roomId; });
    if (mActiveRoom == roomId)
        mActiveRoom.reset();
}
