#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"

#include "gui.h"
#include "memory.h"
#include "types.h"

#define MAX_PARTICLES 8192
#define MAX_PARTITION_PARTICLES 256
#define PARTICLE_BASE_SIZE 5
#define PARTICLE_SPEED 100
#define TARGET_FPS 60

#define SPACE_PARTITIONS 16

typedef struct {
    Vector2 pos;
    Vector2 speed;
    u8 radius;
} Point;

// Group of Point* for world-partition
// based collision detection
typedef struct {
    Point *points[MAX_PARTITION_PARTICLES];
    u32 amount;
} Partition;

// Group of Point* in the world
typedef struct {
    Point *points[MAX_PARTICLES];
    u32 amount;
} Points;

static Points points;

static Partition parts[SPACE_PARTITIONS][SPACE_PARTITIONS] = {0};
static float w;
static float h;
static int PARTITION_SIZE;

static BumpAllocator *tempStorage;

Vector2 getPartitionIndex(Vector2 pos, float half_dim) {
    int x = (int)((pos.x + half_dim) / PARTITION_SIZE);
    int y = (int)((pos.y + half_dim) / PARTITION_SIZE);

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
        Point *p = points.points[i];

        Vector2 idx = getPartitionIndex(p->pos, p->radius);
        int partX = (int)idx.x, partY = (int)idx.y;

        parts[partX][partY].points[parts[partX][partY].amount++] = p;
    }
}

void clearPoints() {
    freeBumpAllocator(tempStorage);
    points.amount = 0;
    memset(points.points, 0, MAX_PARTITION_PARTICLES);
    memset(parts, 0, SPACE_PARTITIONS * SPACE_PARTITIONS * sizeof(Partition));
}
void generatePoints() {
    if (points.amount >= MAX_PARTICLES) {
        printf("Too many particles");
        return;
    }

    for (int i = 0; i < MAX_PARTITION_PARTICLES; i++) {
        Vector2 spawn = {GetRandomValue(-w / 2 + PARTICLE_BASE_SIZE,
                                        w / 2 - PARTICLE_BASE_SIZE),
                         GetRandomValue(-h / 2 + PARTICLE_BASE_SIZE,
                                        h / 2 - PARTICLE_BASE_SIZE)};
        Vector2 speed = {GetRandomValue(-PARTICLE_SPEED, PARTICLE_SPEED),
                         GetRandomValue(-PARTICLE_SPEED, PARTICLE_SPEED)};

        Point *p = (Point *)alloc(tempStorage, sizeof(Point));
        p->speed = speed;
        p->pos = spawn;
        p->radius = PARTICLE_BASE_SIZE;

        points.points[points.amount++] = p;
    }
    updatePartitions();
}

void updateParticlePosition(Point *p, float dt) {
    p->pos.x += p->speed.x * dt;
    p->pos.y += p->speed.y * dt;

    if (p->pos.x + PARTICLE_BASE_SIZE >= w / 2 ||
        p->pos.x - PARTICLE_BASE_SIZE <= -w / 2) {
        p->speed.x = -p->speed.x;
    }
    if (p->pos.y + PARTICLE_BASE_SIZE >= h / 2 ||
        p->pos.y - PARTICLE_BASE_SIZE <= -h / 2) {
        p->speed.y = -p->speed.y;
    }
}

bool checkCollision(Vector2 pos1, float radius1, Vector2 pos2, float radius2) {
    float dx = pos2.x - pos1.x;
    float dy = pos2.y - pos1.y;
    float distanceSquared = dx * dx + dy * dy;
    float sum = radius1 + radius2;
    return distanceSquared <= sum * sum;
}

void resolveCollision(Point *p1, Point *p2) {
    if (!checkCollision(p1->pos, p1->radius, p2->pos, p2->radius)) { return; }

    Vector2 normal = Vector2Subtract(p1->pos, p2->pos);
    float dist = Vector2Length(normal);

    normal = Vector2Normalize(normal);

    Vector2 relativeVelocity = Vector2Subtract(p1->speed, p2->speed);
    float speed = Vector2DotProduct(relativeVelocity, normal);

    if (speed > 0) return;

    float impulse = 2 * speed / (2);
    Vector2 impulseVector = Vector2Scale(normal, impulse);

    p1->speed = Vector2Subtract(p1->speed, impulseVector);
    p2->speed = Vector2Add(p2->speed, impulseVector);
}

void updateParticles(float dt) {
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
                    Point *this = part->points[k];

                    if (!this) break;
                    Point *other = part->points[l];
                    if (!other) continue;
                    resolveCollision(this, other);
                }
            }
        }
    }

    // Movement
    for (int i = 0; i < points.amount; i++) {
        Point *p = points.points[i];
        if (!p) break;
        updateParticlePosition(p, dt);
    }

    updatePartitions();
}

int main(void) {
    tempStorage = NewBumpAlloc(MB(5));

    if (!tempStorage) {
        printf("Failed to init bump allocator\n");
        crash();
    }

    InitWindow(800, 450, "RayLib playground");
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

        float dt = GetFrameTime();

        updateParticles(dt);

        BeginDrawing();
        BeginMode2D(camera);
        {
            ClearBackground(RAYWHITE);

            for (int i = 0; i < points.amount; i++) {
                DrawCircleV(points.points[i]->pos, PARTICLE_BASE_SIZE, RED);
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
