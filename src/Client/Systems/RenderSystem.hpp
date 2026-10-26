#pragma once

#include "Shared/Systems/IRenderSystem.hpp"
#include "Shared/Systems/RegistryRenderingSystem.hpp"

#include <algorithm>
#include <raylib.h>
#include <vector>

class CRenderSystem : public IRenderSystem {
  public:
    void loadTextures(const Ecs::CRegistry& registry) override {
        renderer_.loadTextures(registry);
    }

    void initWindow(int h, int w, std::string title) const override {
        InitWindow(w, h, title.c_str());
    }

    void setTargetFps(int fps) const override {
        SetTargetFPS(fps);
    }

    bool windowShouldClose() const override {
        return WindowShouldClose();
    }

    virtual void clear(std::vector<unsigned char> color) const override {
        if (color.size() != 4)
            return;
        BeginDrawing();
        ClearBackground(Color{.r = color.at(0), .g = color.at(1), .b = color.at(2), .a = color.at(3)});
    }

    void drawRectangle(int x, int y, int w, int h, const std::vector<unsigned char> color) const override {
        if (color.size() != 4)
            return;
        DrawRectangle(x, y, w, h, Color{.r = color.at(0), .g = color.at(1), .b = color.at(2), .a = color.at(3)});
    }

    void drawText(std::string text, int x, int y, int fontSize, const std::vector<unsigned char> color) const override {
        if (color.size() != 4)
            return;
        DrawText(text.c_str(), x, y, fontSize, Color{.r = color.at(0), .g = color.at(1), .b = color.at(2), .a = color.at(3)});
    }

    void endFrame() const override {
        EndDrawing();
    }

    void closeWindow() const override {
        CloseWindow();
    }

    void draw(Ecs::CRegistry& registry) const override {
        renderer_.draw(registry);
        registry.each<SCPosition, SCEntityType, SCHealth>(
            [&](Ecs::SEntity entity, SCPosition& position, SCEntityType& type, SCHealth& health) {
                if (type.value == Game::eEntityKind::PLAYER) {
                    constexpr float barWidth  = 42.0F;
                    constexpr float barHeight = 6.0F;
                    constexpr float maxHealth = 3.0F;
                    const float     healthRatio =
                        static_cast<float>(std::clamp<int>(health.value, 0, 3)) / maxHealth;
                    const auto* shape  = registry.get<SCDrawableShape>(entity);
                    const float radius = shape != nullptr ? shape->radius : 18.0F;
                    const int   barX   = static_cast<int>(position.x - (barWidth / 2.0F));
                    const int   barY   = static_cast<int>(position.y - radius - 14.0F);
                    std::vector<unsigned char> darkGrey = {DARKGRAY.r, DARKGRAY.g, DARKGRAY.b, DARKGRAY.a};
                    std::vector<unsigned char> green = {GREEN.r, GREEN.g, GREEN.b, GREEN.a};
                    std::vector<unsigned char> red = {RED.r, RED.g, RED.b, RED.a};
                    std::vector<unsigned char> white = {RAYWHITE.r, RAYWHITE.g, RAYWHITE.b, RAYWHITE.a};
                    drawRectangle(barX, barY, static_cast<int>(barWidth), static_cast<int>(barHeight),
                        darkGrey);
                    drawRectangle(barX, barY, static_cast<int>(barWidth * healthRatio),
                                  static_cast<int>(barHeight), healthRatio > 0.5F ? green : red);
                    drawText(std::string(TextFormat("HP: %d/3", static_cast<int>(health.value))), barX - 2, barY - 17,
                             12, white);
                }
            });
    }

  private:
    Game::CRegistryRenderingSystem renderer_;
};
