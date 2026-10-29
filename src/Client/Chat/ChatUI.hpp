#pragma once

#include "Shared/Components/ChatInput.hpp"
#include "Shared/Lobby.hpp"
#include "Shared/Systems/ChatSystem.hpp"
#include "Shared/Systems/IRenderSystem.hpp"

#include <deque>

// Draws the chat anchored at the bottom right of the window and feeds the typing
// state. All chat data lives in the ECS registry `Lobby::SLobbyState::chat`
// (SCChatMessage entities and one SCChatInput entity); this class only reads and
// updates it. It is shown while the player is in a room (lobby or in game).
// Clicking the panel focuses the input; Enter sends the message, Enter on an empty
// input or a click elsewhere releases the focus.
class CChatUI {
  public:
    void               draw(IRenderSystem& renderer, Lobby::SLobbyState& state,
                            std::deque<Lobby::SCommand>& commands);

    // True while the chat input captures the keyboard (gameplay keys must be ignored).
    [[nodiscard]] bool typing(Lobby::SLobbyState& state) const {
        return mChatSystem.typing(state.chat);
    }

  private:
    void handleTyping(SCChatInput& input, std::deque<Lobby::SCommand>& commands) const;

    Chat::CChatSystem mChatSystem;
};
