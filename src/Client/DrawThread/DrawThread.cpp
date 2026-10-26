#include "DrawThread.hpp"

#include <raylib.h>

CDrawThread::CDrawThread(SSceneData& sceneData, SPLayerActions& playerActions, std::mutex& sceneMutex,
                         std::atomic_bool& running, std::condition_variable& stopCondition) :
    mSceneData(sceneData), mPlayerActions(playerActions), mtex(sceneMutex), mRunning(running),
    mStopCondition(stopCondition) {}

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
        {
            std::scoped_lock lock(mtex);
            std::size_t      playerId = 0;
            for (std::size_t i = 0; i < SPLayerActions::MaxPlayers; ++i) {
                if (mPlayerActions.connected.at(i)) {
                    playerId = i;
                    break;
                }
            }
            mPlayerActions.states.at(playerId) = SPlayerActionState{
                .up    = IsKeyDown(KEY_UP),
                .down  = IsKeyDown(KEY_DOWN),
                .left  = IsKeyDown(KEY_LEFT),
                .right = IsKeyDown(KEY_RIGHT),
                .fire  = IsKeyDown(KEY_SPACE),
            };
        }
        std::vector<unsigned char> clearColor = {8, 12, 30, 255};
        mRenderSystem.clear(clearColor);
        {
            std::scoped_lock lock(mtex);
            for (int x = 0; x < 1280; x += 80)
                DrawCircle((x + static_cast<int>(mSceneData.tick * 2)) % 1280, 80 + ((x * 37) % 600), 2,
                           RAYWHITE);
            mRenderSystem.loadTextures(mSceneData.renderRegistry);
            mRenderSystem.draw(mSceneData.renderRegistry);
        }
        std::vector<unsigned char> white = {RAYWHITE.r, RAYWHITE.g, RAYWHITE.b, RAYWHITE.a};
        mRenderSystem.drawText(std::string("Arrow keys: move   Space: fire"), 20, 20, 20, white);
        mRenderSystem.endFrame();
    }
    mRenderSystem.closeWindow();
}
