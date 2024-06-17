#include <immintrin.h>

#include <math.h>
#include <raylib.h>
#include <stdlib.h>

#include "./include/globals.h"

#include "./include/sim.h"
#include "./include/types.h"

void updatePartitions() {
    Partition *parts_ptr = &parts[0][0];
    for (int i = 0; i < SPACE_PARTITIONS * SPACE_PARTITIONS; i++) {
        int x = i / SPACE_PARTITIONS;
        int y = i % SPACE_PARTITIONS;
        parts_ptr[x * SPACE_PARTITIONS + y].amount = 0;
    }

    int i = 0;
    for (; i < pts.amount; i += 8) {
        __m256 posX = _mm256_loadu_ps(&pts.positionsX[i]);
        __m256 posY = _mm256_loadu_ps(&pts.positionsY[i]);

        __m256i x = _mm256_cvtps_epi32(_mm256_div_ps(posX, _mm256_set1_ps(PARTITION_SIZE)));
        __m256i y = _mm256_cvtps_epi32(_mm256_div_ps(posY, _mm256_set1_ps(PARTITION_SIZE)));

        x = _mm256_abs_epi32(x);
        y = _mm256_abs_epi32(y);

        _Alignas(32) int x_values[8];
        _Alignas(32) int y_values[8];
        _mm256_store_si256((__m256i *)x_values, x);
        _mm256_store_si256((__m256i *)y_values, y);

        for (int j = 0; j < 8; j++) {
            int x_value = x_values[j];
            int y_value = y_values[j];

            int index = x_value * SPACE_PARTITIONS + y_value;
            Partition *part = &parts_ptr[index];
            part->points[part->amount++] = i + j;
        }
    }

    for (; i < pts.amount; i++) {
        int posX = (int)pts.positionsX[i];
        int posY = (int)pts.positionsY[i];

        int x = abs(posX / PARTITION_SIZE);
        int y = abs(posY / PARTITION_SIZE);

        Partition *part = &parts[x][y];
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
    float radius = pts.radiuses[p];

    float minX = -worldSize.x / 2 + radius, maxX = worldSize.x / 2 - radius;
    bool oobX = (posX + radius >= maxX && speedX > 0) || (posX - radius <= minX && speedX < 0);
    if (oobX) return true;

    float minY = -worldSize.y / 2 + radius, maxY = worldSize.y / 2 - radius;
    bool oobY = (posY + radius >= maxY && speedY > 0) || (posY - radius <= minY && speedY < 0);
    return oobY;
}

bool checkCollisions(u32 p1, u32 p2) {
    float dx = pts.positionsX[p1] - pts.positionsX[p2];
    float dy = pts.positionsY[p1] - pts.positionsY[p2];
    float distanceSquared = dx * dx + dy * dy;
    float sum = pts.radiuses[p1] + pts.radiuses[p2];
    return distanceSquared <= sum * sum;
}

void resolveCollision(int p1, int p2) {
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
    Partition *parts_ptr = &parts[0][0];
    for (int i = 0; i < SPACE_PARTITIONS * SPACE_PARTITIONS; i++) {
        int x = i / SPACE_PARTITIONS, y = i % SPACE_PARTITIONS;
        Partition *part = &parts_ptr[x * SPACE_PARTITIONS + y];
        for (u32 k = 0; k < part->amount; k++) {
            u32 this = part->points[k];
            if (outOfBounds(this)) {
                pts.speedsX[this] = -pts.speedsX[this];
                pts.speedsY[this] = -pts.speedsY[this];
                continue;
            }
            for (u32 l = k + 1; l < part->amount; l++) {
                u32 other = part->points[l];
                if (checkCollisions(this, other)) { resolveCollision(this, other); }
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
