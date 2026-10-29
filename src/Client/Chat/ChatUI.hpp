#pragma once

#include "Shared/Lobby.hpp"
#include "Shared/Systems/IRenderSystem.hpp"

#include <deque>
#include <string>

// Text chat anchored at the bottom right of the window. It is shown while the
// player is in a room (lobby or in game). Clicking the panel focuses the input;
// Enter sends the message, Enter on an empty input or a click elsewhere releases
// the focus.
class CChatUI {
  public:
    void               draw(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                            std::deque<Lobby::SCommand>& commands);

    // True while the chat input captures the keyboard (gameplay keys must be ignored).
    [[nodiscard]] bool focused() const {
        return mFocused;
    }

  private:
    void handleTyping(std::deque<Lobby::SCommand>& commands);

    std::string mInput;
    bool        mFocused = false;
};
