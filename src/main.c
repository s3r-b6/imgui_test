#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"

#include "gui.h"
#include "memory.h"
#include "types.h"

#define MAX_PARTICLES 20480
#define SPACE_PARTITIONS 128
#define NUM_THREADS 8

#define MAX_PARTITION_PARTICLES (MAX_PARTICLES / SPACE_PARTITIONS)
#define POINTS_ADDED 2048

#define BASE_SIZE 0.5
#define MAX_SPEED 100
#define TARGET_FPS 60

#define MASS 1.0
#define RESTITUTION 0.9

// Partition based collision detection
typedef struct {
    u32 amount;
    u32 points[MAX_PARTITION_PARTICLES];
} Partition;

typedef struct {
    u32 amount;
    float *speeds;
    float *positions;
    float *radiuses;
} Points;

static Points points;

static Partition parts[SPACE_PARTITIONS][SPACE_PARTITIONS] = {0};
static int PARTITION_SIZE;

static BumpAllocator *tempStorage;

static float w, h;
static float dt;

Vector2 getPartitionIndex(float posX, float posY, int radius) {
    int x = (int)((posX + radius) / PARTITION_SIZE);
    int y = (int)((posY + radius) / PARTITION_SIZE);

    Vector2 v = {abs(x), abs(y)};
    return v;
}

void updatePartitions() {
    for (int i = 0; i < SPACE_PARTITIONS; i++) {
        for (int j = 0; j < SPACE_PARTITIONS; j++) {
            parts[i][j].amount = 0;
        }
    }

    for (int i = 0; i < points.amount; i++) {
        Vector2 idx = getPartitionIndex(points.positions[(i * 2)],
                                        points.positions[(i * 2) + 1],
                                        points.radiuses[i]);
        int partX = (int)idx.x, partY = (int)idx.y;
        Partition *part = &parts[partX][partY];
        part->points[part->amount++] = i;
    }
}

void clearPoints() {
    freeBumpAllocator(tempStorage);
    points.amount = 0;
    memset(points.positions, 0, points.amount * 2);
    memset(points.speeds, 0, points.amount * 2);
    memset(points.radiuses, 0, points.amount);
    memset(parts, 0, SPACE_PARTITIONS * SPACE_PARTITIONS * sizeof(Partition));
}

void generatePoints() {
    if (points.amount >= MAX_PARTICLES) {
        printf("Too many particles\n");
        return;
    }

    int start = points.amount;
    int end = points.amount + POINTS_ADDED;
    for (int i = start; i < end; i++) {
        u8 radius = (BASE_SIZE + GetRandomValue(4, 6));

        Vector2 spawn = {GetRandomValue(-w / 2 + radius, w / 2 - radius),
                         GetRandomValue(-h / 2 + radius, h / 2 - radius)};
        Vector2 speed = {GetRandomValue(-MAX_SPEED, MAX_SPEED),
                         GetRandomValue(-MAX_SPEED, MAX_SPEED)};

        points.positions[(i * 2)] = spawn.x;
        points.positions[(i * 2) + 1] = spawn.y;
        points.speeds[(i * 2)] = speed.x;
        points.speeds[(i * 2) + 1] = speed.y;
        points.radiuses[(i * 2) + 1] = radius;

        points.amount++;
    }
}

void updatePositions() {
    for (int i = 0; i < points.amount; i++) {
        points.positions[(i * 2)] += points.speeds[(i * 2)] * dt;
        points.positions[(i * 2) + 1] += points.speeds[(i * 2) + 1] * dt;

        if (points.positions[(i * 2)] + BASE_SIZE >= w / 2 ||
            points.positions[(i * 2)] - BASE_SIZE <= -w / 2) {
            points.speeds[(i * 2)] = -points.speeds[(i * 2)];
        }
        if (points.positions[(i * 2) + 1] + BASE_SIZE >= h / 2 ||
            points.positions[(i * 2) + 1] - BASE_SIZE <= -h / 2) {
            points.speeds[(i * 2) + 1] = -points.speeds[(i * 2) + 1];
        }
    }
}

bool checkCollisions(u32 p1, u32 p2) {
    float dx = points.positions[(p1 * 2)] - points.positions[(p2 * 2)];
    float dy = points.positions[(p1 * 2) + 1] - points.positions[(p2 * 2) + 1];
    float distanceSquared = dx * dx + dy * dy;
    float sum = points.radiuses[p1] + points.radiuses[p2];
    return distanceSquared <= sum * sum;
}

void resolveCollision(int p1, int p2) {
    if (!checkCollisions(p1, p2)) { return; }

    float dx = points.positions[p1 * 2] - points.positions[p2 * 2];
    float dy = points.positions[p1 * 2 + 1] - points.positions[p2 * 2 + 1];
    float distanceSquared = dx * dx + dy * dy;
    float sum = points.radiuses[p1] + points.radiuses[p2];

    if (distanceSquared > sum * sum) return;

    float nx = dx / sqrtf(distanceSquared);
    float ny = dy / sqrtf(distanceSquared);

    float dvx = points.speeds[p1 * 2] - points.speeds[p2 * 2];
    float dvy = points.speeds[p1 * 2 + 1] - points.speeds[p2 * 2 + 1];

    float dotProduct = dvx * nx + dvy * ny;

    if (dotProduct > 0) return;

    float impulseScalar =
        (-(1 + RESTITUTION) * dotProduct) / (1 + (1 / MASS) + (1 / MASS));

    points.speeds[p1 * 2] += impulseScalar * (nx / MASS);
    points.speeds[p1 * 2 + 1] += impulseScalar * (ny / MASS);
    points.speeds[p2 * 2] -= impulseScalar * (nx / MASS);
    points.speeds[p2 * 2 + 1] -= impulseScalar * (ny / MASS);
}

void updateParticles() {
    // Check collisions
    // instead of checking every single point against every other point
    //
    // we are checking every single point in a partition against all other
    // points in that partition.
    for (int i = 0; i < SPACE_PARTITIONS; i++) {
        for (int j = 0; j < SPACE_PARTITIONS; j++) {
            Partition *part = &parts[i][j];
            for (u32 k = 0; k < part->amount; k++) {
                for (u32 l = k + 1; l < part->amount; l++) {
                    resolveCollision(k, l);
                }
            }
        }
    }

    updatePositions();
    updatePartitions();
}

int main(void) {
    tempStorage = NewBumpAlloc(MB(5));
    if (!tempStorage) {
        printf("Failed to init bump allocator\n");
        crash();
    }

    points.speeds =
        (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES * 2);
    points.positions =
        (float *)alloc(tempStorage, sizeof(float) * MAX_PARTICLES * 2);
    points.radiuses = (float *)alloc(tempStorage, sizeof(float));

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

            for (int i = 0; i < points.amount; i++) {
                Vector2 pos = {
                    points.positions[i * 2],
                    points.positions[i * 2 + 1],
                };
                DrawCircleV(pos, points.radiuses[i], RED);
            }

            snprintf(dtString, sizeof(dtString), "dt: %f", dt);
            DrawText(dtString, w / 2 - 11 * sizeof(dtString), -h / 2 + 14, 14,
                     BLACK);

            Vector2 b1Pos = {(w / 2) - bSize.x, (h / 2) - bSize.y};
            Vector2 b2Pos = {(w / 2) - bSize.x * 2, (h / 2) - bSize.y};
            if (drawButton("Generate new points", windowPos, b1Pos, bSize,
                           14)) {
                printf("Total points: %d\n", points.amount);
                generatePoints();
            }
            if (drawButton("Delete points", windowPos, b2Pos, bSize, 14)) {
                printf("Deleting %d points\n", points.amount);
                clearPoints();
            }
        }
        EndMode2D();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
