#include <immintrin.h>

#include <math.h>
#include <raylib.h>
#include <stdlib.h>

#include "./include/globals.h"

#include "./include/sim.h"
#include "./include/types.h"

Vector2 getPartitionIndex(u32 idx) {
    float posX = pts.positionsX[idx];
    float posY = pts.positionsY[idx];
    float radius = pts.radiuses[idx];

    int x = (int)((posX + radius) / PARTITION_SIZE);
    int y = (int)((posY + radius) / PARTITION_SIZE);

    Vector2 v = {abs(x), abs(y)};
    return v;
}

void updatePartitions() {
    for (int i = 0; i < SPACE_PARTITIONS; i++) {
        for (int j = 0; j < SPACE_PARTITIONS; j++) { parts[i][j].amount = 0; }
    }

    for (int i = 0; i < pts.amount; i++) {
        Vector2 idx = getPartitionIndex(i);
        int partX = (int)idx.x, partY = (int)idx.y;
        Partition *part = &parts[partX][partY];
        part->points[part->amount++] = i;
    }
}

void updatePositions() {
    if (pts.amount == 0) { return; }

    int i = 0;

    for (; i <= pts.amount - 8; i += 8) {
        __m256 positionsX = _mm256_loadu_ps(&pts.positionsX[i]);
        __m256 positionsY = _mm256_loadu_ps(&pts.positionsY[i]);

        __m256 speedsX = _mm256_loadu_ps(&pts.speedsX[i]);
        __m256 speedsY = _mm256_loadu_ps(&pts.speedsY[i]);

        __m256 deltaT = _mm256_set1_ps(dt);

        positionsX = _mm256_add_ps(positionsX, _mm256_mul_ps(speedsX, deltaT));
        positionsY = _mm256_add_ps(positionsY, _mm256_mul_ps(speedsY, deltaT));

        _mm256_storeu_ps(&pts.positionsX[i], positionsX);
        _mm256_storeu_ps(&pts.positionsY[i], positionsY);
    }

    // remaining points
    for (; i < pts.amount; i++) {
        pts.positionsX[i] += pts.speedsX[i] * dt;
        pts.positionsY[i] += pts.speedsY[i] * dt;
    }
}

bool outOfBounds(u32 p) {
    float posX = pts.positionsX[p], posY = pts.positionsY[p];
    float speedX = pts.speedsX[p], speedY = pts.speedsY[p];
    float r = pts.radiuses[p];

    bool oobX = (posX + r >= w / 2 && speedX > 0) || (posX - r <= -w / 2 && speedX < 0);
    bool oobY = (posY + r >= h / 2 && speedY > 0) || (posY - r <= -h / 2 && speedY < 0);

    return oobX || oobY;
}

bool checkCollisions(u32 p1, u32 p2) {
    float dx = pts.positionsX[p1] - pts.positionsX[p2];
    float dy = pts.positionsY[p1] - pts.positionsY[p2];
    float distanceSquared = dx * dx + dy * dy;
    float sum = pts.radiuses[p1] + pts.radiuses[p2];
    return distanceSquared <= sum * sum;
}

void resolveCollision(int p1, int p2) {
    if (!checkCollisions(p1, p2)) { return; }

    float dx = pts.positionsX[p1] - pts.positionsX[p2];
    float dy = pts.positionsY[p1] - pts.positionsY[p2];
    float distanceSquared = dx * dx + dy * dy;
    float sumRadii = pts.radiuses[p1] + pts.radiuses[p2];

    if (distanceSquared > sumRadii * sumRadii) return;

    float epsilon = 1e-5f;
    float sqrtDist = sqrtf(distanceSquared) + epsilon;
    float nx = dx / sqrtDist;
    float ny = dy / sqrtDist;

    float dvx = pts.speedsX[p1] - pts.speedsX[p2];
    float dvy = pts.speedsY[p1] - pts.speedsY[p2];

    float dotProduct = dvx * nx + dvy * ny;

    if (dotProduct > 0) return;

    pts.speedsX[p1] += -dotProduct * nx;
    pts.speedsY[p1] += -dotProduct * ny;
    pts.speedsX[p2] -= -dotProduct * nx;
    pts.speedsY[p2] -= -dotProduct * ny;
}

void solveCollisions() {
    // Check collisions
    // instead of checking every single point against every other point
    //
    // we are checking every single point in a partition against all other
    // points in that partition.
    for (int i = 0; i < SPACE_PARTITIONS; i++) {
        for (int j = 0; j < SPACE_PARTITIONS; j++) {
            Partition part = parts[i][j];
            for (u32 k = 0; k < part.amount; k++) {
                u32 this = part.points[k];
                if (outOfBounds(this)) {
                    pts.speedsX[this] = -pts.speedsX[this];
                    pts.speedsY[this] = -pts.speedsY[this];
                    continue;
                }
                for (u32 l = k + 1; l < part.amount; l++) {
                    u32 other = part.points[l];
                    resolveCollision(this, other);
                }
            }
        }
    }
}

void updateParticles() {
    double start = GetTime();
    updatePartitions();
    double endParts = GetTime();
    solveCollisions();
    double endColls = GetTime();
    updatePositions();
    double end = GetTime();

    double totalMS = (end - start) * 1000;
    double totalParts = (endParts - start) * 1000;
    double totalColls = (endColls - endParts) * 1000;
    double totalPos = (end - endColls) * 1000;

    printf("Updated %hu particles in: \n"
           " TOTAL: %.2fms\n"
           " - Partitions: %.2fms\n"
           " - Collisions: %.2fms\n"
           " - Positions: %.2fms\n",
           pts.amount, totalMS, totalParts, totalColls, totalPos);
}
