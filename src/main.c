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
    pts.speedsX = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.speedsY = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.positionsX = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.positionsY = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.radiuses = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.colors = (u8 *)alloc(tempStorage, sizeof(u8) * MAX_PARTICLES);
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
        u8 r = BASE_SIZE + GetRandomValue(4, 6);

        pts.positionsX[i] = (float)GetRandomValue(-w / 2 + r, w / 2 - r);
        pts.positionsY[i] = (float)GetRandomValue(-h / 2 + r, h / 2 - r);

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
    PARTITION_SIZE = w / SPACE_PARTITIONS;

    Vector2 bSize = {100, 40};

    Vector2 center = {w / 2, h / 2};
    Camera2D camera = {.offset = center, .zoom = 1};

    generatePoints();

    char dtString[14];
    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();
        Vector2 windowPos = {mousePos.x - center.x, mousePos.y - center.y};

        dt = GetFrameTime();

        updateParticles();

        BeginDrawing();
        BeginMode2D(camera);
        {
            ClearBackground(RAYWHITE);

            for (int i = 0; i < pts.amount; i++) {
                Vector2 pos = {pts.positionsX[i], pts.positionsY[i]};

                DrawCircleV(pos, pts.radiuses[i], colors[pts.colors[i]]);
            }

            snprintf(dtString, sizeof(dtString), "dt: %f", dt);
            DrawText(dtString, w / 2 - 11 * sizeof(dtString), -h / 2 + 14, 14, BLACK);

            Vector2 b1Pos = {(w / 2) - bSize.x, (h / 2) - bSize.y};
            Vector2 b2Pos = {(w / 2) - bSize.x * 2, (h / 2) - bSize.y};
            if (drawButton("Generate new points", windowPos, b1Pos, bSize, 14)) {
                printf("Total points: %d\n", pts.amount);
                generatePoints();
            }
            if (drawButton("Delete points", windowPos, b2Pos, bSize, 14)) {
                printf("Deleting %d points\n", pts.amount);
                clearPoints();
            }
        }
        EndMode2D();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
