#include "Client/Client.hpp"
#include <cstdio>
#include <exception>
#include <iostream>
#include <raylib.h>

static const char* Host = "127.0.0.1";
static const int   W = 960, H = 640;

int                main() {
    InitWindow(W, H, "R-Type Client");
    SetTargetFPS(60);

    try {
        asio::io_context io;
        Net::CClient     client(io);
        client.connect(Host);

        float otherX[16] = {};
        float otherY[16] = {};

        while (!WindowShouldClose()) {
            Vector2 mouse = GetMousePosition();
            io.poll();
            client.poll();

            BeginDrawing();
            ClearBackground(RAYWHITE);

            if (client.connected()) {
                client.sendState(mouse.x, mouse.y);
                int n = client.broadcast(otherX, otherY, 16);

                DrawText("CONNECTED", 10, 10, 20, GREEN);
                char buf[80];
                snprintf(buf, sizeof(buf), "Player #%d  %.0f, %.0f", client.playerId(), mouse.x,
                         mouse.y);
                DrawText(buf, 10, 40, 16, DARKGRAY);

                for (int i = 0; i < n; i++) {
                    Color c = (i == 0) ? BLUE : GRAY;
                    DrawCircle(static_cast<int>(otherX[i]), static_cast<int>(otherY[i]), 20.0, c);
                }
                DrawCircle(static_cast<int>(mouse.x), static_cast<int>(mouse.y), 20.0, GOLD);
            } else {
                DrawText("Connecting...", (W / 2) - 60, H / 2, 20, GRAY);
            }

            EndDrawing();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Client] fatal: " << e.what() << '\n';
        CloseWindow();
        return 1;
    }

    CloseWindow();
    return 0;
}
