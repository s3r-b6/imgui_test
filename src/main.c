#include <stdio.h>

#include "raylib.h"
#include "raymath.h"

#define TARGET_FPS 60

bool drawButton(const char *text, Vector2 mousePos, Vector2 pos, Vector2 dim,
                float fontSize) {
    bool isContained = pos.x <= mousePos.x && pos.x + dim.x >= mousePos.x &&
                       pos.y <= mousePos.y && pos.y + dim.y >= mousePos.y;

    Color c = GRAY;

    if (isContained) c = ColorTint(c, BLACK);
    DrawRectangle(pos.x, pos.y, dim.x, dim.y, c);
    DrawText(text, pos.x + (fontSize / 2), pos.y + fontSize, fontSize,
             RAYWHITE);

    return isContained && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

int main(void) {
    InitWindow(800, 450, "RayLib playground");
    SetTargetFPS(TARGET_FPS);

    float w = GetScreenWidth();
    float h = GetScreenHeight();

    Vector2 pos = {0};
    Vector2 center = {w / 2, h / 2};
    Camera2D camera = {.offset = center, .zoom = 1};

    float radius = 110.f;
    float angle = 0.f;
    int speed = 4;

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();
        Vector2 windowPos = {mousePos.x - center.x, mousePos.y - center.y};

        float dt = GetFrameTime();
        angle += dt * speed;

        pos.x = radius * cos(angle);
        pos.y = radius * sin(angle);

        BeginDrawing();
        BeginMode2D(camera);
        {
            ClearBackground(RAYWHITE);

            Vector2 bPos = {-50, -20}, bSize = {100, 40};
            if (drawButton("Test Button", windowPos, bPos, bSize, 14)) {
                speed = (speed + 1) % 6;
                printf("New speed: %d\n", speed);
            }

            DrawRing(pos, 15, 20, 0, 360, 100, RED);
        }
        EndMode2D();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
