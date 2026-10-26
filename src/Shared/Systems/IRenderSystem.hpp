#pragma once

#include "Shared/ECS/Registry.hpp"
#include <vector>
class IRenderSystem {
    public:
        virtual ~IRenderSystem() = default;
        virtual void loadTextures(const Ecs::CRegistry &registry) = 0;
        virtual void drawRectangle(int x, int y, int w, int h, const std::vector<unsigned char> color) const = 0;
        virtual void drawText(std::string text, int x, int y, int fontSize, const std::vector<unsigned char> color) const = 0;
        virtual void initWindow(int h, int w, std::string title) const = 0;
        virtual void setTargetFps(int fps) const = 0;
        virtual bool windowShouldClose() const = 0;
        virtual void clear(std::vector<unsigned char> color) const = 0;
        virtual void endFrame() const = 0;
        virtual void closeWindow() const = 0;
        virtual void draw(Ecs::CRegistry &registry) const = 0;
};
