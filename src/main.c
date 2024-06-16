#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"

#include "gui.h"
#include "memory.h"
#include "types.h"

#define MAX_PARTICLES 20480
#define SPACE_PARTITIONS 128
#define NUM_THREADS 8

#define MAX_PARTITION_PARTICLES (MAX_PARTICLES / SPACE_PARTITIONS)
#define POINTS_ADDED 2048

#define BASE_SIZE 2
#define MAX_SPEED 100
#define TARGET_FPS 60

typedef struct {
    u8 radius;
    Vector2 pos;
    Vector2 speed;
} Point;

// Group of Point* for world-partition
// based collision detection
typedef struct {
    u32 amount;
    Point *points[MAX_PARTITION_PARTICLES];
} Partition;

// Group of Point* in the world
typedef struct {
    u32 amount;
    Point *points[MAX_PARTICLES];
} Points;

typedef struct {
    int start_row;
    int end_row;
    int start_col;
    int end_col;
} ThreadData;

static Points points;

static Partition parts[SPACE_PARTITIONS][SPACE_PARTITIONS] = {0};
static float w;
static float h;
static int PARTITION_SIZE;

static BumpAllocator *tempStorage;

static float dt;

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

        Partition *part = &parts[partX][partY];
        part->points[part->amount++] = p;
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
        printf("Too many particles\n");
        return;
    }

    for (int i = 0; i < POINTS_ADDED; i++) {
        u8 radius = (BASE_SIZE + GetRandomValue(1, 3));

        Vector2 spawn = {GetRandomValue(-w / 2 + radius, w / 2 - radius),
                         GetRandomValue(-h / 2 + radius, h / 2 - radius)};
        Vector2 speed = {GetRandomValue(-MAX_SPEED, MAX_SPEED),
                         GetRandomValue(-MAX_SPEED, MAX_SPEED)};

        Point *p = (Point *)alloc(tempStorage, sizeof(Point));
        p->speed = speed;
        p->pos = spawn;
        p->radius = radius;

        points.points[points.amount++] = p;
    }
}

void updateParticlePosition(Point *p) {
    p->pos.x += p->speed.x * dt;
    p->pos.y += p->speed.y * dt;

    if (p->pos.x + BASE_SIZE >= w / 2 || p->pos.x - BASE_SIZE <= -w / 2) {
        p->speed.x = -p->speed.x;
    }
    if (p->pos.y + BASE_SIZE >= h / 2 || p->pos.y - BASE_SIZE <= -h / 2) {
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

void *resolveCollisionsTask(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    for (int i = data->start_row; i < data->end_row; i++) {
        for (int j = data->start_col; j < data->end_col; j++) {
            Partition *part = &parts[i][j];
            for (u32 k = 0; k < part->amount; k++) {
                for (u32 l = k + 1; l < part->amount; l++) {
                    Point *this = part->points[k];
                    Point *other = part->points[l];

                    if (!checkCollision(this->pos, this->radius, other->pos,
                                        other->radius))
                        continue;

                    resolveCollision(this, other);
                }
            }
        }
    }
    pthread_exit(NULL);
}

void *updatePositionsTask(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    for (int i = data->start_row; i < data->end_row; i++) {
        Point *p = points.points[i];
        updateParticlePosition(p);
    }
    pthread_exit(NULL);
}

void updateParticlesThreaded() {
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];
    int rows_per_thread = SPACE_PARTITIONS / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == NUM_THREADS - 1)
                                     ? SPACE_PARTITIONS
                                     : (i + 1) * rows_per_thread;
        thread_data[i].start_col = 0;
        thread_data[i].end_col = SPACE_PARTITIONS;

        if (pthread_create(&threads[i], NULL, resolveCollisionsTask,
                           (void *)&thread_data[i])) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            exit(2);
        }
    }

    int points_per_thread = points.amount / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].start_row = i * points_per_thread;
        thread_data[i].end_row = (i + 1) * points_per_thread;
        if (pthread_create(&threads[i], NULL, updatePositionsTask,
                           (void *)&thread_data[i])) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            exit(2);
        }
    }

    updatePartitions();
}

int main(void) {
    tempStorage = NewBumpAlloc(MB(5));
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

        updateParticlesThreaded();

        BeginDrawing();
        BeginMode2D(camera);
        {
            ClearBackground(RAYWHITE);

            for (int i = 0; i < points.amount; i++) {
                Point *p = points.points[i];
                DrawCircleV(p->pos, p->radius, RED);
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
