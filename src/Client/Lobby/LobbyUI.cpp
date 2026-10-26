#include "LobbyUI.hpp"

#include <raylib.h>

#include <cstddef>
#include <string>

namespace {

    constexpr int      ScreenWidth  = 1280;
    constexpr int      ScreenHeight = 720;

    constexpr SColor   White{.r = 255, .g = 255, .b = 255, .a = 255};
    constexpr SColor   LightGray{.r = 200, .g = 200, .b = 200, .a = 255};
    constexpr SColor   Gray{.r = 130, .g = 130, .b = 130, .a = 255};
    constexpr SColor   Red{.r = 230, .g = 41, .b = 55, .a = 255};
    constexpr SColor   Gold{.r = 255, .g = 203, .b = 0, .a = 255};
    constexpr SColor   PanelColor{.r = 40, .g = 50, .b = 80, .a = 255};
    constexpr SColor   PanelHoverColor{.r = 60, .g = 75, .b = 115, .a = 255};
    constexpr SColor   ButtonColor{.r = 50, .g = 70, .b = 120, .a = 255};
    constexpr SColor   ButtonHoverColor{.r = 80, .g = 120, .b = 200, .a = 255};
    constexpr SColor   ButtonDisabledColor{.r = 60, .g = 60, .b = 60, .a = 255};
    constexpr SColor   InputColor{.r = 20, .g = 25, .b = 40, .a = 255};
    constexpr SColor   BorderColor{.r = 90, .g = 110, .b = 160, .a = 255};

    [[nodiscard]] bool hovered(int x, int y, int width, int height) {
        return CheckCollisionPointRec(GetMousePosition(),
                                      Rectangle{
                                          .x      = static_cast<float>(x),
                                          .y      = static_cast<float>(y),
                                          .width  = static_cast<float>(width),
                                          .height = static_cast<float>(height),
                                      });
    }

    void drawPanel(IRenderSystem& renderer, int x, int y, int width, int height, SColor fill,
                   SColor border) {
        renderer.drawRectangle(x, y, width, height, border);
        renderer.drawRectangle(x + 1, y + 1, width - 2, height - 2, fill);
    }

    [[nodiscard]] std::string playerLine(const Room::SPlayer& player, std::uint8_t masterId,
                                         std::uint8_t selfId) {
        std::string line = player.name;
        if (player.id == masterId)
            line += "  (leader)";
        if (player.id == selfId)
            line += "  (you)";
        return line;
    }

} // namespace

void CLobbyUI::draw(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                    std::deque<Lobby::SCommand>& commands) {
    renderer.drawText("R-TYPE", (ScreenWidth - MeasureText("R-TYPE", 60)) / 2, 30, 60, White);

    switch (state.phase) {
        case Lobby::eLobbyPhase::CONNECTING: drawConnecting(renderer); break;
        case Lobby::eLobbyPhase::NAME_ENTRY: drawNameEntry(renderer, state, commands); break;
        case Lobby::eLobbyPhase::ROOM_LIST: drawRoomList(renderer, state, commands); break;
        case Lobby::eLobbyPhase::IN_ROOM: drawRoom(renderer, state, commands); break;
        case Lobby::eLobbyPhase::ERROR: drawError(renderer, state); break;
        case Lobby::eLobbyPhase::IN_GAME: break;
    }

    if (!state.message.empty())
        renderer.drawText(state.message, 20, ScreenHeight - 40, 20, Red);
}

void CLobbyUI::drawConnecting(IRenderSystem& renderer) const {
    const std::string text = "Connecting to server...";
    renderer.drawText(text, (ScreenWidth - MeasureText(text.c_str(), 24)) / 2, ScreenHeight / 2, 24,
                      LightGray);
}

void CLobbyUI::drawNameEntry(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                             std::deque<Lobby::SCommand>& commands) {
    const std::string label = "Enter your name (1-8 printable characters):";
    renderer.drawText(label, (ScreenWidth - MeasureText(label.c_str(), 24)) / 2, (ScreenHeight / 2) - 80,
                      24, White);

    const int inputX = (ScreenWidth / 2) - 150;
    const int inputY = (ScreenHeight / 2) - 30;
    drawTextInput(renderer, inputX, inputY, 300, 44, mNameInput, Room::MaxPlayerNameLength);

    const bool canJoin = !mNameInput.empty();
    if (drawButton(renderer, (ScreenWidth / 2) - 60, (ScreenHeight / 2) + 30, 120, 44, "Join",
                   canJoin) ||
        (canJoin && IsKeyPressed(KEY_ENTER))) {
        commands.push_back(Lobby::SCommand{.type = Lobby::eClientCommand::JOIN, .text = mNameInput});
    }

    if (!state.playerName.empty()) {
        const std::string joined = "Joined as " + state.playerName;
        renderer.drawText(joined, (ScreenWidth - MeasureText(joined.c_str(), 20)) / 2,
                          (ScreenHeight / 2) + 100, 20, LightGray);
    }
}

void CLobbyUI::drawRoomList(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                            std::deque<Lobby::SCommand>& commands) {
    renderer.drawText("Player: " + state.playerName, 20, 110, 20, LightGray);
    renderer.drawText("Rooms", 20, 145, 30, White);

    int y = 190;
    if (state.rooms.empty()) {
        renderer.drawText("No rooms yet. Create one!", 20, y, 20, Gray);
        y += 40;
    } else {
        for (const auto& room : state.rooms) {
            const bool isHovered = hovered(20, y, ScreenWidth - 40, 50);
            drawPanel(renderer, 20, y, ScreenWidth - 40, 50, isHovered ? PanelHoverColor : PanelColor,
                      BorderColor);
            const std::string line = room.name + "   " + std::to_string(room.playerCount) + "/" +
                std::to_string(room.maxPlayers) + "   " +
                (room.state == Room::eState::WAITING ? "waiting" : "in game") +
                "   leader: " + room.masterName;
            renderer.drawText(line, 30, y + 15, 20, White);
            const bool selectable =
                room.state == Room::eState::WAITING && room.playerCount < room.maxPlayers;
            if (isHovered && selectable && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                commands.push_back(Lobby::SCommand{
                    .type   = Lobby::eClientCommand::SELECT_ROOM,
                    .text   = {},
                    .roomId = room.id,
                });
            }
            y += 60;
        }
    }

    if (mCreateRoomOpen) {
        renderer.drawText("Room name (1-24 printable characters):", 20, y + 10, 20, White);
        drawTextInput(renderer, 20, y + 40, 320, 44, mRoomNameInput, Room::MaxRoomNameLength);
        if (drawButton(renderer, 20, y + 100, 120, 44, "Create", !mRoomNameInput.empty())) {
            commands.push_back(
                Lobby::SCommand{.type = Lobby::eClientCommand::CREATE_ROOM, .text = mRoomNameInput});
            mCreateRoomOpen = false;
            mRoomNameInput.clear();
        }
        if (drawButton(renderer, 155, y + 100, 120, 44, "Cancel")) {
            mCreateRoomOpen = false;
            mRoomNameInput.clear();
        }
    } else {
        if (drawButton(renderer, 20, y + 10, 160, 44, "Create Room"))
            mCreateRoomOpen = true;
        if (drawButton(renderer, 195, y + 10, 120, 44, "Refresh"))
            commands.push_back(
                Lobby::SCommand{.type = Lobby::eClientCommand::REFRESH_ROOMS, .text = {}});
    }

    renderer.drawText("Click a room to join it.", 20, ScreenHeight - 70, 18, Gray);
}

void CLobbyUI::drawRoom(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                        std::deque<Lobby::SCommand>& commands) {
    if (!state.currentRoom)
        return;
    const auto& room = *state.currentRoom;
    renderer.drawText("Room: " + room.name, 20, 110, 30, White);
    renderer.drawText("Players: " + std::to_string(room.players.size()) + "/" +
                          std::to_string(Room::MaxPlayersPerRoom),
                      20, 150, 20, LightGray);

    int y = 195;
    for (const auto& player : room.players) {
        const bool isMaster = player.id == room.masterId;
        renderer.drawText(playerLine(player, room.masterId, state.playerId), 30, y, 22,
                          isMaster ? Gold : White);
        y += 32;
    }

    const bool isMaster = room.masterId == state.playerId;
    if (isMaster) {
        if (drawButton(renderer, 20, y + 30, 200, 50, "Launch Game"))
            commands.push_back(Lobby::SCommand{.type = Lobby::eClientCommand::LAUNCH_GAME, .text = {}});
    } else {
        renderer.drawText("Waiting for the room leader to launch the game...", 20, y + 40, 20,
                          LightGray);
    }

    if (drawButton(renderer, 20, y + 100, 140, 44, "Leave Room"))
        commands.push_back(Lobby::SCommand{.type = Lobby::eClientCommand::LEAVE_ROOM, .text = {}});

    renderer.drawText("Room state: " +
                          std::string(room.state == Room::eState::WAITING ? "waiting" : "in game"),
                      20, ScreenHeight - 70, 18, Gray);
}

void CLobbyUI::drawError(IRenderSystem& renderer, const Lobby::SLobbyState& state) const {
    renderer.drawText("Connection error", 20, 120, 30, Red);
    renderer.drawText(state.message, 20, 165, 20, White);
    renderer.drawText("Close the window to exit.", 20, 205, 18, Gray);
}

bool CLobbyUI::drawButton(IRenderSystem& renderer, int x, int y, int width, int height,
                          const std::string& text, bool enabled) const {
    const bool isHovered = enabled && hovered(x, y, width, height);
    drawPanel(renderer, x, y, width, height,
              !enabled ? ButtonDisabledColor : (isHovered ? ButtonHoverColor : ButtonColor), White);
    const int textWidth = MeasureText(text.c_str(), 20);
    renderer.drawText(text, x + ((width - textWidth) / 2), y + ((height - 20) / 2), 20,
                      enabled ? White : Gray);
    return enabled && isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void CLobbyUI::drawTextInput(IRenderSystem& renderer, int x, int y, int width, int height,
                             std::string& text, std::size_t maxLength) const {
    drawPanel(renderer, x, y, width, height, InputColor, White);

    int character = GetCharPressed();
    while (character > 0) {
        if (character >= 32 && character <= 126 && text.size() < maxLength)
            text.push_back(static_cast<char>(character));
        character = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !text.empty())
        text.pop_back();

    renderer.drawText(text, x + 10, y + 12, 20, White);
    if (static_cast<int>(GetTime() * 2.0) % 2 == 0) {
        const int cursorX = x + 10 + MeasureText(text.c_str(), 20);
        renderer.drawRectangle(cursorX, y + 12, 2, 20, White);
    }
}
