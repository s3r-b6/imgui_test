#pragma once

#include <raylib.h>

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
