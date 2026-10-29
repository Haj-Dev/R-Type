#include "Server/Room/RoomManager.hpp"
#include "Shared/LobbyProtocol.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

TEST(LobbyProtocol, JoinRoundTripsPlayerName) {
    const auto frame = Net::makeJoin("Alice");
    ASSERT_FALSE(frame.empty());

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::JOIN);
    EXPECT_EQ(payloadLength, Room::MaxPlayerNameLength);

    std::string name;
    ASSERT_TRUE(Net::readJoin(frame.data() + Net::LobbyHeaderSize, payloadLength, name));
    EXPECT_EQ(name, "Alice");
}

TEST(LobbyProtocol, JoinRejectsInvalidNames) {
    EXPECT_TRUE(Net::makeJoin("").empty());
    EXPECT_TRUE(Net::makeJoin("TooLongName").empty());
    EXPECT_TRUE(Net::makeJoin("Bad\nName").empty());

    const auto  frame = Net::makeJoin("Alice");
    std::string name;
    EXPECT_FALSE(Net::readJoin(frame.data() + Net::LobbyHeaderSize, 7, name));
}

TEST(LobbyProtocol, RoomListRoundTrips) {
    const std::vector<Room::SSummary> rooms{
        Room::SSummary{
            .id          = 1,
            .name        = "Alpha",
            .playerCount = 2,
            .maxPlayers  = 4,
            .state       = Room::eState::WAITING,
            .masterName  = "Alice",
        },
        Room::SSummary{
            .id          = 2,
            .name        = "Beta",
            .playerCount = 4,
            .maxPlayers  = 4,
            .state       = Room::eState::IN_GAME,
            .masterName  = "Bob",
        },
    };
    const auto         frame = Net::makeRoomList(rooms);

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::ROOM_LIST);

    std::vector<Room::SSummary> decoded;
    ASSERT_TRUE(Net::readRoomList(frame.data() + Net::LobbyHeaderSize, payloadLength, decoded));
    ASSERT_EQ(decoded.size(), 2U);
    EXPECT_EQ(decoded.at(0).id, 1);
    EXPECT_EQ(decoded.at(0).name, "Alpha");
    EXPECT_EQ(decoded.at(0).playerCount, 2);
    EXPECT_EQ(decoded.at(0).state, Room::eState::WAITING);
    EXPECT_EQ(decoded.at(0).masterName, "Alice");
    EXPECT_EQ(decoded.at(1).id, 2);
    EXPECT_EQ(decoded.at(1).state, Room::eState::IN_GAME);
}

TEST(LobbyProtocol, RoomInfoRoundTrips) {
    const Room::SInfo room{
        .id       = 7,
        .name     = "Squad",
        .state    = Room::eState::WAITING,
        .masterId = 1,
        .players  = {{.id = 1, .name = "Alice"}, {.id = 3, .name = "Bob"}},
    };
    const auto         frame = Net::makeRoomInfo(Net::eLobbyCommand::ROOM_JOINED, room);

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::ROOM_JOINED);

    Room::SInfo decoded;
    ASSERT_TRUE(Net::readRoomInfo(frame.data() + Net::LobbyHeaderSize, payloadLength, decoded));
    EXPECT_EQ(decoded.id, 7);
    EXPECT_EQ(decoded.name, "Squad");
    EXPECT_EQ(decoded.masterId, 1);
    ASSERT_EQ(decoded.players.size(), 2U);
    EXPECT_EQ(decoded.players.at(0).id, 1);
    EXPECT_EQ(decoded.players.at(0).name, "Alice");
    EXPECT_EQ(decoded.players.at(1).id, 3);
    EXPECT_EQ(decoded.players.at(1).name, "Bob");
}

TEST(LobbyProtocol, RejectionRoundTrips) {
    const auto         frame = Net::makeRoomRejected(Room::eRejectReason::ROOM_FULL, "Room is full");

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::ROOM_REJECTED);

    Room::eRejectReason reason;
    std::string         message;
    ASSERT_TRUE(
        Net::readRoomRejected(frame.data() + Net::LobbyHeaderSize, payloadLength, reason, message));
    EXPECT_EQ(reason, Room::eRejectReason::ROOM_FULL);
    EXPECT_EQ(message, "Room is full");
}

TEST(LobbyProtocol, HeaderRejectsBadMagicAndOversizedPayload) {
    auto               frame = Net::makeRoomListRequest();

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::ROOM_LIST_REQUEST);
    EXPECT_EQ(payloadLength, 0);

    auto badMagic  = frame;
    badMagic.at(0) = 'X';
    EXPECT_FALSE(Net::readLobbyHeader(badMagic.data(), badMagic.size(), command, payloadLength));

    auto oversized  = frame;
    oversized.at(4) = 0xFF;
    oversized.at(5) = 0xFF;
    EXPECT_FALSE(Net::readLobbyHeader(oversized.data(), oversized.size(), command, payloadLength));
}

TEST(LobbyProtocol, ChatSendRoundTrips) {
    const auto frame = Net::makeChatSend(SCChatMessage{0, 0, {}, "hello team!"});
    ASSERT_FALSE(frame.empty());

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::CHAT_SEND);

    SCChatMessage message;
    ASSERT_TRUE(Net::readChatSend(frame.data() + Net::LobbyHeaderSize, payloadLength, message));
    EXPECT_EQ(message.text, "hello team!");
}

TEST(LobbyProtocol, ChatSendRejectsInvalidText) {
    const auto send = [](const std::string& text) {
        return Net::makeChatSend(SCChatMessage{0, 0, {}, text});
    };
    EXPECT_TRUE(send("").empty());
    EXPECT_TRUE(send("bad\ntext").empty());
    EXPECT_TRUE(send(std::string(Chat::MaxTextLength + 1, 'a')).empty());
    EXPECT_FALSE(send(std::string(Chat::MaxTextLength, 'a')).empty());

    const auto    frame = send("hi");
    SCChatMessage message;
    EXPECT_FALSE(Net::readChatSend(frame.data() + Net::LobbyHeaderSize, 2, message));
}

TEST(LobbyProtocol, ChatMessageRoundTrips) {
    const auto frame = Net::makeChatMessage(SCChatMessage{7, 2, "Alice", "gg everyone"});
    ASSERT_FALSE(frame.empty());

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::CHAT_MESSAGE);

    SCChatMessage decoded;
    ASSERT_TRUE(Net::readChatMessage(frame.data() + Net::LobbyHeaderSize, payloadLength, decoded));
    EXPECT_EQ(decoded.sequence, 7U);
    EXPECT_EQ(decoded.senderId, 2);
    EXPECT_EQ(decoded.sender, "Alice");
    EXPECT_EQ(decoded.text, "gg everyone");
    EXPECT_FALSE(Net::readChatMessage(frame.data() + Net::LobbyHeaderSize, payloadLength - 1, decoded));
}

TEST(RoomManager, CreateJoinAndLaunch) {
    CRoomManager manager;
    EXPECT_TRUE(manager.setPlayerName(0, "Alice").ok);
    EXPECT_TRUE(manager.setPlayerName(1, "Bob").ok);

    const auto created = manager.createRoom(0, "Alpha");
    ASSERT_TRUE(created.ok);
    const auto room = manager.roomOfPlayer(0);
    ASSERT_TRUE(room.has_value());
    EXPECT_EQ(room->masterId, 0);
    EXPECT_EQ(room->players.size(), 1U);

    const auto joined = manager.joinRoom(1, room->id);
    ASSERT_TRUE(joined.ok);
    const auto updated = manager.roomOfPlayer(0);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->players.size(), 2U);

    EXPECT_TRUE(manager.isMaster(0));
    EXPECT_FALSE(manager.isMaster(1));

    const auto launched = manager.launch(0);
    ASSERT_TRUE(launched.ok);
    ASSERT_TRUE(manager.activeRoom().has_value());
    EXPECT_EQ(*manager.activeRoom(), room->id);
    const auto info = manager.roomInfo(room->id);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->state, Room::eState::IN_GAME);
}

TEST(LobbyProtocol, GameStartedRoundTripsRoomAndGameplayId) {
    const auto         frame = Net::makeGameStarted(7, 3);

    Net::eLobbyCommand command;
    std::uint16_t      payloadLength = 0;
    ASSERT_TRUE(Net::readLobbyHeader(frame.data(), frame.size(), command, payloadLength));
    EXPECT_EQ(command, Net::eLobbyCommand::GAME_STARTED);

    std::uint16_t roomId     = 0;
    std::uint8_t  gameplayId = 0;
    ASSERT_TRUE(
        Net::readGameStarted(frame.data() + Net::LobbyHeaderSize, payloadLength, roomId, gameplayId));
    EXPECT_EQ(roomId, 7);
    EXPECT_EQ(gameplayId, 3);
}

TEST(RoomManager, RejectsInvalidOperations) {
    CRoomManager manager;
    EXPECT_FALSE(manager.createRoom(0, "Alpha").ok);
    EXPECT_TRUE(manager.setPlayerName(0, "Alice").ok);
    EXPECT_TRUE(manager.setPlayerName(1, "Bob").ok);
    EXPECT_TRUE(manager.setPlayerName(2, "Cara").ok);
    EXPECT_TRUE(manager.setPlayerName(3, "Dan").ok);
    EXPECT_TRUE(manager.setPlayerName(4, "Eve").ok);

    ASSERT_TRUE(manager.createRoom(0, "Alpha").ok);
    const auto roomId = manager.roomIdOfPlayer(0);
    ASSERT_TRUE(roomId.has_value());
    EXPECT_FALSE(manager.createRoom(0, "Beta").ok);
    EXPECT_FALSE(manager.joinRoom(1, 999).ok);
    EXPECT_TRUE(manager.joinRoom(1, *roomId).ok);
    EXPECT_TRUE(manager.joinRoom(2, *roomId).ok);
    EXPECT_TRUE(manager.joinRoom(3, *roomId).ok);
    EXPECT_FALSE(manager.joinRoom(3, *roomId).ok);
    EXPECT_FALSE(manager.joinRoom(4, *roomId).ok);
    EXPECT_FALSE(manager.launch(1).ok);
    ASSERT_TRUE(manager.launch(0).ok);
    EXPECT_FALSE(manager.launch(0).ok);
}

TEST(RoomManager, RejectsSecondActiveGame) {
    CRoomManager manager;
    for (std::uint8_t id = 0; id < SPLayerActions::MaxPlayers; ++id)
        ASSERT_TRUE(manager.setPlayerName(id, "P" + std::to_string(id)).ok);

    ASSERT_TRUE(manager.createRoom(0, "Alpha").ok);
    ASSERT_TRUE(manager.createRoom(1, "Beta").ok);
    ASSERT_TRUE(manager.launch(0).ok);
    EXPECT_FALSE(manager.launch(1).ok);
}

TEST(RoomManager, LeavingPromotesMasterAndRemovesEmptyRoom) {
    CRoomManager manager;
    ASSERT_TRUE(manager.setPlayerName(0, "Alice").ok);
    ASSERT_TRUE(manager.setPlayerName(1, "Bob").ok);
    ASSERT_TRUE(manager.createRoom(0, "Alpha").ok);
    const auto roomId = manager.roomIdOfPlayer(0);
    ASSERT_TRUE(roomId.has_value());
    ASSERT_TRUE(manager.joinRoom(1, *roomId).ok);

    ASSERT_TRUE(manager.leaveRoom(0).ok);
    const auto room = manager.roomInfo(*roomId);
    ASSERT_TRUE(room.has_value());
    EXPECT_EQ(room->masterId, 1);
    EXPECT_EQ(room->players.size(), 1U);

    ASSERT_TRUE(manager.leaveRoom(1).ok);
    EXPECT_FALSE(manager.roomInfo(*roomId).has_value());
    EXPECT_FALSE(manager.activeRoom().has_value());
}

TEST(RoomManager, RemovingPlayerClearsMembership) {
    CRoomManager manager;
    ASSERT_TRUE(manager.setPlayerName(0, "Alice").ok);
    ASSERT_TRUE(manager.createRoom(0, "Alpha").ok);
    const auto roomId = manager.roomIdOfPlayer(0);
    ASSERT_TRUE(roomId.has_value());
    manager.removePlayer(0);
    EXPECT_FALSE(manager.hasPlayer(0));
    EXPECT_FALSE(manager.roomInfo(*roomId).has_value());
    EXPECT_FALSE(manager.activeRoom().has_value());
}
