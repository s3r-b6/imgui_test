#include <math.h>
#include <stdlib.h>

#include "./include/globals.h"

#include "./include/sim.h"
#include "./include/types.h"

Vector2 getPartitionIndex(u32 idx) {
    float posX = pts.positions[(idx * 2)];
    float posY = pts.positions[(idx * 2) + 1];
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
    for (int i = 0; i < pts.amount; i++) {
        float val = pts.positions[(i * 2)];
        pts.positions[(i * 2)] += pts.speeds[(i * 2)] * dt;
        pts.positions[(i * 2) + 1] += pts.speeds[(i * 2) + 1] * dt;
    }
}

bool outOfBounds(u32 p) {
    float r = pts.radiuses[p];
    float posX = pts.positions[p * 2];
    float posY = pts.positions[p * 2 + 1];
    float speedX = pts.speeds[p * 2];
    float speedY = pts.speeds[p * 2 + 1];

    bool oobX = (posX + r >= w / 2 && speedX > 0) || (posX - r <= -w / 2 && speedX < 0);
    bool oobY = (posY + r >= h / 2 && speedY > 0) || (posY - r <= -h / 2 && speedY < 0);

    return oobX || oobY;
}

bool checkCollisions(u32 p1, u32 p2) {
    float dx = pts.positions[(p1 * 2)] - pts.positions[(p2 * 2)];
    float dy = pts.positions[(p1 * 2) + 1] - pts.positions[(p2 * 2) + 1];
    float distanceSquared = dx * dx + dy * dy;
    float sum = pts.radiuses[p1] + pts.radiuses[p2];
    return distanceSquared <= sum * sum;
}

void resolveCollision(int p1, int p2) {
    if (!checkCollisions(p1, p2)) { return; }

    float dx = pts.positions[p1 * 2] - pts.positions[p2 * 2];
    float dy = pts.positions[p1 * 2 + 1] - pts.positions[p2 * 2 + 1];
    float distanceSquared = dx * dx + dy * dy;
    float sumRadii = pts.radiuses[p1] + pts.radiuses[p2];

    if (distanceSquared > sumRadii * sumRadii) return;

    float epsilon = 1e-5f;
    float sqrtDist = sqrtf(distanceSquared) + epsilon;
    float nx = dx / sqrtDist;
    float ny = dy / sqrtDist;

    float dvx = pts.speeds[p1 * 2] - pts.speeds[p2 * 2];
    float dvy = pts.speeds[p1 * 2 + 1] - pts.speeds[p2 * 2 + 1];

    float dotProduct = dvx * nx + dvy * ny;

    if (dotProduct > 0) return;

    pts.speeds[p1 * 2] += -dotProduct * nx;
    pts.speeds[p1 * 2 + 1] += -dotProduct * ny;
    pts.speeds[p2 * 2] -= -dotProduct * nx;
    pts.speeds[p2 * 2 + 1] -= -dotProduct * ny;
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
                    pts.speeds[this * 2] = -pts.speeds[this * 2];
                    pts.speeds[this * 2 + 1] = -pts.speeds[this * 2 + 1];
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
    updatePartitions();
    solveCollisions();
    updatePositions();
}
