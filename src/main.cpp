/*******************************************************************************************
*
*   raylib [models] example - waving cubes
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 3.7
*
*   Example contributed by Codecat (@codecat) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Codecat (@codecat) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include <cmath>

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main() {
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth  = 800;
    const int screenHeight = 450;

    SetTraceLogLevel(LOG_WARNING);

    InitWindow(screenWidth, screenHeight, "raylib [models] example - waving cubes");

    // Initialize the camera
    Camera3D camera = {};
    camera.position = Vector3{.x = 30.0f, .y = 20.0f, .z = 30.0f}; // Camera position
    camera.target   = Vector3{.x = 0.0f, .y = 0.0f, .z = 0.0f};    // Camera looking at point
    camera.up   = Vector3{.x = 0.0f, .y = 1.0f, .z = 0.0f}; // Camera up vector (rotation towards target)
    camera.fovy = 70.0f;                                    // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera projection type

    // Specify the amount of blocks in each direction
    const int numBlocks = 15;

    // Catppuccin Mocha accent colors, used to color the cubes instead of a rainbow HSV sweep
    const Color catppuccin_mocha_accents[] = {
        {245, 224, 220, 0xFF}, // Rosewater
        {242, 205, 205, 0xFF}, // Flamingo
        {245, 194, 231, 0xFF}, // Pink
        {203, 166, 247, 0xFF}, // Mauve
        {243, 139, 168, 0xFF}, // Red
        {235, 160, 172, 0xFF}, // Maroon
        {250, 179, 135, 0xFF}, // Peach
        {249, 226, 175, 0xFF}, // Yellow
        {166, 227, 161, 0xFF}, // Green
        {148, 226, 213, 0xFF}, // Teal
        {137, 220, 235, 0xFF}, // Sky
        {116, 199, 236, 0xFF}, // Sapphire
        {137, 180, 250, 0xFF}, // Blue
        {180, 190, 254, 0xFF}, // Lavender
    };
    const int numAccentColors = sizeof(catppuccin_mocha_accents) / sizeof(catppuccin_mocha_accents[0]);

    SetTargetFPS(30);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        double time = GetTime();

        // Calculate time scale for cube position and size
        float scale = (2.0f + (float)sin(time)) * 0.7f;

        // Move camera around the scene
        double cameraTime = time * 0.3;
        camera.position.x = (float)cos(cameraTime) * 40.0f;
        camera.position.z = (float)sin(cameraTime) * 40.0f;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        Color catppuccin_background = {30, 30, 46, 0xFF}; // Catppuccin Mocha background color

        ClearBackground(catppuccin_background);

        BeginMode3D(camera);

        DrawGrid(10, 5.0f);

        for (int x = 0; x < numBlocks; x++) {
            for (int y = 0; y < numBlocks; y++) {
                for (int z = 0; z < numBlocks; z++) {
                    // Scale of the blocks depends on x/y/z positions
                    float blockScale = (x + y + z) / 30.0f;

                    // Scatter makes the waving effect by adding blockScale over time
                    float scatter = sinf((blockScale * 20.0f) + (float)(time * 4.0f));

                    // Calculate the cube position
                    Vector3 cubePos = {
                        .x = ((float)(x - ((float)numBlocks / 2)) * (scale * 3.0f)) + scatter,
                        .y = ((float)(y - ((float)numBlocks / 2)) * (scale * 2.0f)) + scatter,
                        .z = ((float)(z - ((float)numBlocks / 2)) * (scale * 3.0f)) + scatter};

                    // Pick a Catppuccin Mocha accent color depending on cube position
                    Color cubeColor = catppuccin_mocha_accents[(x + y + z) % numAccentColors];

                    // Calculate cube size
                    float cubeSize = (2.4f - scale) * blockScale;

                    // And finally, draw the cube!
                    DrawCube(cubePos, cubeSize, cubeSize, cubeSize, cubeColor);
                }
            }
        }

        EndMode3D();

        DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }
}
