#include <stdio.h>

#include "raylib.h"

#include "./include/gui.h"
#include "./include/memory.h"
#include "./include/types.h"

#include "./include/globals.h"
#include "./include/sim.h"

#include "sim.c"

void allocPoints() {
    pts.amount = 0;
    pts.speedsX = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.speedsY = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.positionsX = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.positionsY = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.radiuses = (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES);
    pts.colors = (Color *)alloc(tempStorage, sizeof(Color) * MAX_PARTICLES);
}

void clearPoints() {
    freeBumpAllocator(tempStorage);
    memset(parts, 0, SPACE_PARTITIONS * SPACE_PARTITIONS * sizeof(Partition));
    allocPoints();
}

void generatePoints() {
    if (pts.amount >= MAX_PARTICLES) {
        printf("Too many particles\n");
        return;
    }

    Color c;

    const int end = pts.amount + POINTS_ADDED;
    for (int i = pts.amount; i < end; ++i) {
        switch (GetRandomValue(1, 7)) {
        case 1: c = GREEN; break;
        case 2: c = PURPLE; break;
        case 3: c = BLUE; break;
        case 4: c = BLACK; break;
        case 5: c = ORANGE; break;
        case 6: c = BROWN; break;
        case 7: c = RED; break;
        }

        u8 r = BASE_SIZE + GetRandomValue(4, 6);

        pts.positionsX[i] = (float)GetRandomValue(-w / 2 + r, w / 2 - r);
        pts.positionsY[i] = (float)GetRandomValue(-h / 2 + r, h / 2 - r);

        pts.speedsX[i] = (float)GetRandomValue(-MAX_SPEED, MAX_SPEED);
        pts.speedsY[i] = (float)GetRandomValue(-MAX_SPEED, MAX_SPEED);

        pts.radiuses[i] = r;
        pts.colors[i] = c;
    }

    pts.amount += POINTS_ADDED;
}

int main(void) {
    tempStorage = NewBumpAlloc(MB(50));
    if (!tempStorage) {
        printf("Failed to init bump allocator\n");
        crash();
    }

    allocPoints();

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
                DrawCircleV(pos, pts.radiuses[i], pts.colors[i]);
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
