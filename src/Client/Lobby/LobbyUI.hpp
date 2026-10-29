#pragma once

#include "Shared/Lobby.hpp"
#include "Shared/Systems/IRenderSystem.hpp"

#include <cstddef>
#include <deque>
#include <string>

class CLobbyUI {
  public:
    void draw(IRenderSystem& renderer, const Lobby::SLobbyState& state,
              std::deque<Lobby::SCommand>& commands);

  private:
    void               drawConnecting(IRenderSystem& renderer) const;
    void               drawServerList(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                                      std::deque<Lobby::SCommand>& commands);
    void               drawNameEntry(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                                     std::deque<Lobby::SCommand>& commands);
    void               drawRoomList(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                                    std::deque<Lobby::SCommand>& commands);
    void               drawRoom(IRenderSystem& renderer, const Lobby::SLobbyState& state,
                                std::deque<Lobby::SCommand>& commands);
    void               drawError(IRenderSystem& renderer, const Lobby::SLobbyState& state) const;

    [[nodiscard]] bool drawButton(IRenderSystem& renderer, int x, int y, int width, int height,
                                  const std::string& text, bool enabled = true) const;
    void drawTextInput(IRenderSystem& renderer, int x, int y, int width, int height, std::string& text,
                       std::size_t maxLength) const;

    std::string mNameInput;
    std::string mRoomNameInput;
    bool        mCreateRoomOpen = false;
};
