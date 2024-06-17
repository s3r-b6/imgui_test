#include <stdio.h>

#include "raylib.h"

#include "./include/gui.h"
#include "./include/memory.h"
#include "./include/types.h"

#include "./include/globals.h"
#include "./include/sim.h"

#include "sim.c"

Color colors[10] = {};

void allocPoints() {
    pts.speedsX = (float *)alloc(tempStorage, 32, sizeof(float) * MAX_PARTICLES);
    pts.speedsY = (float *)alloc(tempStorage, 32, sizeof(float) * MAX_PARTICLES);
    pts.positionsX = (float *)alloc(tempStorage, 32, sizeof(float) * MAX_PARTICLES);
    pts.positionsY = (float *)alloc(tempStorage, 32, sizeof(float) * MAX_PARTICLES);
    pts.radiuses = (float *)alloc(tempStorage, 32, sizeof(float) * MAX_PARTICLES);
    pts.colors = (u8 *)alloc(tempStorage, 32, sizeof(u8) * MAX_PARTICLES);
}

void clearPoints() {
    freeBumpAllocator(tempStorage);
    pts.amount = 0;
    memset(parts, 0, SPACE_PARTITIONS * SPACE_PARTITIONS * sizeof(Partition));
}

void generatePoints() {
    if (unlikely(pts.amount == 0)) {
        allocPoints();
    } else if (unlikely(pts.amount >= MAX_PARTICLES)) {
        printf("Too many particles\n");
        return;
    }

    const int end = pts.amount + POINTS_ADDED;
    for (int i = pts.amount; i < end; ++i) {
        u8 r = BASE_SIZE + GetRandomValue(3, 4);

        pts.positionsX[i] = (float)GetRandomValue(-worldSize.x / 2 + r, worldSize.x / 2 - r);
        pts.positionsY[i] = (float)GetRandomValue(-worldSize.y / 2 + r, worldSize.y / 2 - r);

        pts.speedsX[i] = (float)GetRandomValue(-MAX_SPEED, MAX_SPEED);
        pts.speedsY[i] = (float)GetRandomValue(-MAX_SPEED, MAX_SPEED);

        pts.radiuses[i] = r;
        pts.colors[i] = GetRandomValue(0, 10);
    }

    pts.amount += POINTS_ADDED;
}

int main(void) {
    // https://coolors.co/palette/001219-005f73-0a9396-94d2bd-e9d8a6-ee9b00-ca6702-bb3e03-ae2012-9b2226
    colors[0] = GetColor(0x001219ff);
    colors[1] = GetColor(0x005f73ff);
    colors[2] = GetColor(0x0a9396ff);
    colors[3] = GetColor(0x94d2bdff);
    colors[4] = GetColor(0xe9d8a6ff);
    colors[5] = GetColor(0xee9b00ff);
    colors[6] = GetColor(0xca6702ff);
    colors[7] = GetColor(0xbb3e03ff);
    colors[8] = GetColor(0xae2012ff);
    colors[9] = GetColor(0x9b2226ff);

    tempStorage = NewBumpAlloc(MB(50));
    if (!tempStorage) {
        printf("Failed to init bump allocator\n");
        crash();
    }

    InitWindow(1280, 720, "RayLib playground");
    SetTargetFPS(TARGET_FPS);

    w = GetScreenWidth(), h = GetScreenHeight();
    PARTITION_SIZE = worldSize.x / SPACE_PARTITIONS;

    Vector2 bSize = {100, 40};
    Vector2 center = {worldSize.x / 2, worldSize.y / 2};
    Camera2D camera = {.offset = w / 2, h / 2, .zoom = 1};

    generatePoints();

    Image circleImg = LoadImage("assets/white-circle-no-outline.png");
    ImageMipmaps(&circleImg);
    Texture2D circleTex = LoadTextureFromImage(circleImg);
    // SetTextureFilter(circleTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(circleImg);

    char dtString[14];
    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();
        dt = GetFrameTime();

        updateParticles();

        { // Camera controls
            // zoom
            camera.zoom += GetMouseWheelMove() * 0.10f;
            if (camera.zoom < 0.5f) {
                camera.zoom = 0.5f;
            } else if (camera.zoom > 1.5f) {
                camera.zoom = 1.5f;
            }

            // drag to move
            static int startX;
            static int startY;
            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                int newX = GetMouseX();
                int newY = GetMouseY();
                if (startX != 0 && startY != 0) {
                    int diffX = newX - startX;
                    int diffY = newY - startY;

                    camera.offset.x += diffX;
                    camera.offset.y += diffY;

                    printf("Camera moved x: %d y: %d\n", newX - startX, newY - startY);
                }
                startX = newX;
                startY = newY;
            }

            if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) { startX = 0, startY = 0; }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode2D(camera);
        { // WORLD_PASS
            Vector2 origin = {0, 0};
            Rectangle src = {0, 0, circleTex.width, circleTex.height};
            for (int i = 0; i < pts.amount / 2; i += 2) {
                // Basic loop unrolling
                float posX1 = pts.positionsX[i], posY1 = pts.positionsY[i],
                      radius1 = pts.radiuses[i];
                float posX2 = pts.positionsX[i + 1], posY2 = pts.positionsY[i + 1],
                      radius2 = pts.radiuses[i + 1];

                Rectangle dest1 = {posX1 - radius1, posY1 - radius1, radius1 * 2, radius1 * 2};
                Rectangle dest2 = {posX2 - radius2, posY2 - radius2, radius2 * 2, radius2 * 2};

                DrawTexturePro(circleTex, src, dest1, origin, 0, colors[pts.colors[i]]);
                DrawTexturePro(circleTex, src, dest2, origin, 0, colors[pts.colors[i + 1]]);
            }
        }
        EndMode2D();

        { // UI pass
            snprintf(dtString, sizeof(dtString), "dt: %f", dt);

            Vector2 textSize = MeasureTextEx(GetFontDefault(), dtString, 14, 1);
            int textPosX = w - textSize.x - 10, textPosY = 10;

            DrawText(dtString, textPosX, textPosY, 14, BLACK);

            Vector2 b1Pos = {w - bSize.x, h - bSize.y};
            Vector2 b2Pos = {w - bSize.x * 2, h - bSize.y};
            if (drawButton("Generate new points", mousePos, b1Pos, bSize, 14)) {
                generatePoints();
                printf("Total points: %d\n", pts.amount);
            }
            if (drawButton("Delete points", mousePos, b2Pos, bSize, 14)) {
                printf("Deleting %d points\n", pts.amount);
                clearPoints();
            }
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
