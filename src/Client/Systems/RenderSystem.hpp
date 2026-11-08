#pragma once

#include "Shared/Components/LevelState.hpp"
#include "Shared/Systems/IRenderSystem.hpp"
#include "Shared/Systems/RegistryRenderingSystem.hpp"
#include "Shared/Components/Weapon.hpp"

#include <algorithm>
#include <cmath>
#include <raylib.h>

class CRenderSystem : public IRenderSystem {
  public:
    void loadTextures(const Ecs::CRegistry& registry) override {
        renderer_.loadTextures(registry);
    }

    void initWindow(int w, int h, std::string title) const override {
        InitWindow(w, h, title.c_str());
    }

    void setTargetFps(int fps) const override {
        SetTargetFPS(fps);
    }

    bool windowShouldClose() const override {
        return WindowShouldClose();
    }

    void clear(SColor color) const override {
        BeginDrawing();
        ClearBackground(toRaylib(color));
    }

    void drawRectangle(int x, int y, int w, int h, const SColor color) const override {
        DrawRectangle(x, y, w, h, toRaylib(color));
    }

    void drawText(std::string text, int x, int y, int fontSize, const SColor color) const override {
        DrawText(text.c_str(), x, y, fontSize, toRaylib(color));
    }

    void endFrame() const override {
        EndDrawing();
    }

    void closeWindow() const override {
        CloseWindow();
    }

    void drawCircle(int x, int y, int r, SColor color) const override {
        DrawCircle(x, y, r, toRaylib(color));
    }

    void draw(Ecs::CRegistry& registry, std::uint64_t currentTick) const override {
        renderer_.draw(registry, currentTick);
        registry.each<SCLevelState>([](Ecs::SEntity, const SCLevelState& state) {
            DrawRectangle(1050, 20, 210, 46, Fade(BLACK, 0.65F));
            DrawText(TextFormat("LEVEL %u / %u", static_cast<unsigned>(state.level),
                                static_cast<unsigned>(state.totalLevels)),
                     1062, 32, 20, RAYWHITE);
            if (state.transitionRemaining > 0.0F) {
                DrawText(TextFormat("AREA CLEAR - NEXT IN %d",
                                    static_cast<int>(std::ceil(state.transitionRemaining))),
                         930, 78, 20, GOLD);
            } else if (state.finished) {
                DrawRectangle(350, 300, 580, 110, Fade(BLACK, 0.8F));
                DrawText("CAMPAIGN COMPLETE", 405, 338, 32, GOLD);
            }
        });
        registry.each<SCPosition, SCEntityType, SCHealth>([&](Ecs::SEntity entity, SCPosition& position,
                                                              SCEntityType& type, SCHealth& health) {
            if (type.value == Game::eEntityKind::PLAYER) {
                constexpr float barWidth  = 42.0F;
                constexpr float barHeight = 6.0F;
                constexpr float maxHealth = 3.0F;
                const float     healthRatio =
                    static_cast<float>(std::clamp<int>(health.value, 0, 3)) / maxHealth;
                const auto*  shape    = registry.get<SCDrawableShape>(entity);
                const float  radius   = shape != nullptr ? shape->radius : 18.0F;
                const int    barX     = static_cast<int>(position.x - (barWidth / 2.0F));
                const int    barY     = static_cast<int>(position.y - radius - 14.0F);
                const SColor darkGrey = fromRaylib(DARKGRAY);
                const SColor green    = fromRaylib(GREEN);
                const SColor red      = fromRaylib(RED);
                const SColor white    = fromRaylib(RAYWHITE);
                drawRectangle(barX, barY, static_cast<int>(barWidth), static_cast<int>(barHeight),
                              darkGrey);
                drawRectangle(barX, barY, static_cast<int>(barWidth * healthRatio),
                              static_cast<int>(barHeight), healthRatio > 0.5F ? green : red);
                drawText(std::string(TextFormat("HP: %d/3", static_cast<int>(health.value))), barX - 2,
                         barY - 17, 12, white);
                if (const auto* weapon = registry.get<SCWeapon>(entity)) {
                    const char* weaponName = weapon->mode == Game::eWeaponMode::HEAVY ?
                        "HEAVY" :
                        (weapon->mode == Game::eWeaponMode::SPREAD ? "SPREAD" : nullptr);
                    if (weaponName != nullptr)
                        drawText(std::string(
                                     TextFormat("%s %ds", weaponName,
                                                static_cast<int>(std::ceil(weapon->durationRemaining)))),
                                 static_cast<int>(position.x) - 35, static_cast<int>(position.y) + 22,
                                 12, white);
                }
            }
        });
    }

  private:
    static Color toRaylib(const SColor& c) {
        return Color{.r = c.r, .g = c.g, .b = c.b, .a = c.a};
    }

    static SColor fromRaylib(const Color& c) {
        return SColor{.r = c.r, .g = c.g, .b = c.b, .a = c.a};
    }

    Game::CRegistryRenderingSystem renderer_;
};
