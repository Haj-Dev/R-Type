#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "Client/Chat/ChatUI.hpp"
#include "Client/Lobby/LobbyUI.hpp"
#include "Shared/Lobby.hpp"
#include "Shared/SceneData.hpp"
#include "Shared/PlayerActions.hpp"
#include "Client/Systems/RenderSystem.hpp"

class CDrawThread {
  public:
    CDrawThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                std::atomic_bool& running, std::condition_variable& stopCondition,
                Lobby::SLobbyState& lobbyState, std::deque<Lobby::SCommand>& lobbyCommands);
    ~CDrawThread();

    CDrawThread(const CDrawThread&)            = delete;
    CDrawThread& operator=(const CDrawThread&) = delete;

    void         start();
    void         join();

  private:
    void                         run();

    SSceneData&                  mSceneData;
    SPLayerActions&              mPlayerActions;
    std::mutex&                  mtex;
    std::atomic_bool&            mRunning;
    std::condition_variable&     mStopCondition;
    Lobby::SLobbyState&          mLobbyState;
    std::deque<Lobby::SCommand>& mLobbyCommands;
    std::thread                  mThread;
    CRenderSystem                mRenderSystem;
    CLobbyUI                     mLobbyUI;
    CChatUI                      mChatUI;
};
