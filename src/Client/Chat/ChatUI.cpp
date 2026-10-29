#include "ChatUI.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

    constexpr int    ScreenWidth      = 1280;
    constexpr int    ScreenHeight     = 720;
    constexpr int    Margin           = 10;
    constexpr int    PanelWidth       = 360;
    constexpr int    InputHeight      = 30;
    constexpr int    Padding          = 8;
    constexpr int    FontSize         = 16;
    constexpr int    LineHeight       = 20;
    constexpr int    VisibleLines     = 10;
    constexpr int    LogHeight        = (VisibleLines * LineHeight) + (2 * Padding);
    constexpr int    PanelGap         = 4;

    constexpr SColor White{.r = 255, .g = 255, .b = 255, .a = 255};
    constexpr SColor Gray{.r = 130, .g = 130, .b = 130, .a = 255};
    constexpr SColor Gold{.r = 255, .g = 203, .b = 0, .a = 255};
    constexpr SColor SkyBlue{.r = 102, .g = 191, .b = 255, .a = 255};
    constexpr SColor BorderColor{.r = 90, .g = 110, .b = 160, .a = 255};
    constexpr SColor InputColor{.r = 20, .g = 25, .b = 40, .a = 255};
    constexpr SColor LogColor{.r = 10, .g = 14, .b = 30, .a = 190};

    struct SLine {
        std::string text;
        std::size_t nameLength = 0;
        SColor      nameColor  = White;
    };

    [[nodiscard]] bool hovered(int x, int y, int width, int height) {
        return CheckCollisionPointRec(GetMousePosition(),
                                      Rectangle{
                                          .x      = static_cast<float>(x),
                                          .y      = static_cast<float>(y),
                                          .width  = static_cast<float>(width),
                                          .height = static_cast<float>(height),
                                      });
    }

    [[nodiscard]] std::vector<std::string> wrap(const std::string& text, int maxWidth) {
        std::vector<std::string> lines;
        std::string              current;
        for (const char character : text) {
            current.push_back(character);
            if (MeasureText(current.c_str(), FontSize) <= maxWidth)
                continue;
            const auto space = current.find_last_of(' ');
            if (space != std::string::npos && space > 0) {
                lines.push_back(current.substr(0, space));
                current = current.substr(space + 1);
            } else if (current.size() > 1) {
                const char last = current.back();
                current.pop_back();
                lines.push_back(current);
                current.assign(1, last);
            }
        }
        if (!current.empty())
            lines.push_back(current);
        return lines;
    }

    [[nodiscard]] std::vector<SLine> buildLines(const std::vector<const SCChatMessage*>& messages,
                                                std::uint8_t                             selfId) {
        constexpr int      textWidth = PanelWidth - (2 * Padding);
        std::vector<SLine> lines;
        for (const auto* message : messages) {
            const std::string namePart  = message->sender + ":";
            const SColor      nameColor = message->senderId == selfId ? Gold : SkyBlue;
            bool              first     = true;
            for (auto& line : wrap(namePart + " " + message->text, textWidth)) {
                const bool hasName = first && line.starts_with(namePart);
                lines.push_back(SLine{
                    .text       = std::move(line),
                    .nameLength = hasName ? namePart.size() : 0,
                    .nameColor  = nameColor,
                });
                first = false;
            }
        }
        return lines;
    }

    void drawBox(IRenderSystem& renderer, int x, int y, int width, int height, SColor fill,
                 SColor border) {
        renderer.drawRectangle(x, y, width, height, border);
        renderer.drawRectangle(x + 1, y + 1, width - 2, height - 2, fill);
    }

} // namespace

void CChatUI::handleTyping(SCChatInput& input, std::deque<Lobby::SCommand>& commands) const {
    int character = GetCharPressed();
    while (character > 0) {
        if (character >= 32 && character <= 126 && input.text.size() < Chat::MaxTextLength)
            input.text.push_back(static_cast<char>(character));
        character = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !input.text.empty())
        input.text.pop_back();
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (input.text.empty()) {
            input.focused = false;
        } else {
            commands.push_back(
                Lobby::SCommand{.type = Lobby::eClientCommand::SEND_CHAT, .text = input.text});
            input.text.clear();
        }
    }
}

void CChatUI::draw(IRenderSystem& renderer, Lobby::SLobbyState& state,
                   std::deque<Lobby::SCommand>& commands) {
    auto& input = mChatSystem.input(state.chat);
    if (state.phase != Lobby::eLobbyPhase::IN_ROOM && state.phase != Lobby::eLobbyPhase::IN_GAME) {
        input.focused = false;
        return;
    }

    const int panelX = ScreenWidth - PanelWidth - Margin;
    const int inputY = ScreenHeight - Margin - InputHeight;
    const int logY   = inputY - PanelGap - LogHeight;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const bool clicked = hovered(panelX, logY, PanelWidth, LogHeight + PanelGap + InputHeight);
        if (clicked && !input.focused) {
            while (GetCharPressed() > 0);
        }
        input.focused = clicked;
    }
    if (input.focused)
        handleTyping(input, commands);

    const auto lines = buildLines(mChatSystem.history(state.chat), state.playerId);
    if (!lines.empty() || input.focused) {
        if (input.focused)
            drawBox(renderer, panelX, logY, PanelWidth, LogHeight, LogColor,
                Gold);
        const std::size_t shown = std::min(lines.size(), static_cast<std::size_t>(VisibleLines));
        int               y     = logY + Padding + (static_cast<int>(VisibleLines - shown) * LineHeight);
        for (std::size_t index = lines.size() - shown; index < lines.size(); ++index) {
            const auto& line = lines.at(index);
            if (line.nameLength > 0) {
                const std::string name = line.text.substr(0, line.nameLength);
                renderer.drawText(name, panelX + Padding, y, FontSize, line.nameColor);
                renderer.drawText(line.text.substr(line.nameLength),
                                  panelX + Padding + MeasureText(name.c_str(), FontSize), y, FontSize,
                                  White);
            } else {
                renderer.drawText(line.text, panelX + Padding, y, FontSize, White);
            }
            y += LineHeight;
        }
    }

    drawBox(renderer, panelX, inputY, PanelWidth, InputHeight, InputColor,
            input.focused ? Gold : BorderColor);
    if (!input.focused && input.text.empty()) {
        renderer.drawText("Click here to chat...", panelX + Padding, inputY + ((InputHeight - FontSize) / 2),
                          FontSize, Gray);
        return;
    }

    std::string visible = input.text;
    while (!visible.empty() && MeasureText(visible.c_str(), FontSize) > PanelWidth - (3 * Padding))
        visible.erase(0, 1);
    const int textY = inputY + ((InputHeight - FontSize) / 2);
    renderer.drawText(visible, panelX + Padding, textY, FontSize, White);
    if (input.focused && static_cast<int>(GetTime() * 2.0) % 2 == 0) {
        renderer.drawRectangle(panelX + Padding + MeasureText(visible.c_str(), FontSize) + 1, textY, 2,
                               FontSize, White);
    }
}
