#include "DrawThread.hpp"

#include <raylib.h>

CDrawThread::CDrawThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                         std::atomic_bool& running, std::condition_variable& stopCondition,
                         Lobby::SLobbyState& lobbyState, std::deque<Lobby::SCommand>& lobbyCommands) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition), mLobbyState(lobbyState), mLobbyCommands(lobbyCommands) {}

CDrawThread::~CDrawThread() {
    join();
}

void CDrawThread::start() {
    if (!mThread.joinable()) {
        mThread = std::thread(&CDrawThread::run, this);
    }
}

void CDrawThread::join() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

void CDrawThread::run() {
    SetTraceLogLevel(LOG_WARNING);
    mRenderSystem.initWindow(1280, 720, "R-Type");
    mRenderSystem.setTargetFps(60);
    mRenderSystem.loadTextures(mSceneData.renderRegistry);

    while (mRunning.load()) {
        if (mRenderSystem.windowShouldClose()) {
            mRunning.store(false);
            mStopCondition.notify_all();
            break;
        }

        bool inGame = false;
        {
            std::scoped_lock lock(mtex);
            inGame = mLobbyState.phase == Lobby::eLobbyPhase::IN_GAME;
        }

        mRenderSystem.clear(SColor{.r = 8, .g = 12, .b = 30, .a = 255});
        if (!inGame) {
            std::scoped_lock lock(mtex);
            mLobbyUI.draw(mRenderSystem, mLobbyState, mLobbyCommands);
            mChatUI.draw(mRenderSystem, mLobbyState, mLobbyCommands);
        } else {
            {
                std::scoped_lock lock(mtex);
                std::size_t      playerId = 0;
                for (std::size_t i = 0; i < SPLayerActions::MaxPlayers; ++i) {
                    if (mPlayerActions.connected.at(i)) {
                        playerId = i;
                        break;
                    }
                }
                const bool typing = mChatUI.typing(mLobbyState);
                mPlayerActions.states.at(playerId) = SPlayerActionState{
                    .up    = !typing && IsKeyDown(KEY_UP),
                    .down  = !typing && IsKeyDown(KEY_DOWN),
                    .left  = !typing && IsKeyDown(KEY_LEFT),
                    .right = !typing && IsKeyDown(KEY_RIGHT),
                    .fire  = !typing && IsKeyDown(KEY_SPACE),
                };
            }
            {
                std::scoped_lock lock(mtex);
                for (int x = 0; x < 1280; x += 80)
                    mRenderSystem.drawCircle((x + static_cast<int>(mSceneData.tick * 2)) % 1280,
                                             80 + ((x * 37) % 600), 2,
                                             SColor{.r = 255, .g = 255, .b = 255, .a = 255});
                mRenderSystem.loadTextures(mSceneData.renderRegistry);
                mRenderSystem.draw(mSceneData.renderRegistry);
            }
            mRenderSystem.drawText("Arrow keys: move   Space: fire", 20, 20, 20,
                                   SColor{.r = 255, .g = 255, .b = 255, .a = 255});
            {
                std::scoped_lock lock(mtex);
                mChatUI.draw(mRenderSystem, mLobbyState, mLobbyCommands);
            }
        }
        mRenderSystem.endFrame();
    }
    mRenderSystem.closeWindow();
}
