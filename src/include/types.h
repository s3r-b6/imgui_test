#pragma once

#include <raylib.h>
#include <stdint.h>

#define u64 uint64_t
#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t
#define i64 int64_t
#define i32 int32_t
#define i16 int16_t
#define i8 int8_t

#define MAX_PARTICLES 20480 * 8
#define NUM_THREADS 8

#define POINTS_ADDED 2048 * 8

#define BASE_SIZE 0.2
#define MAX_SPEED 100
#define TARGET_FPS 60

#define SPACE_PARTITIONS 256
#define MAX_PARTITION_PARTICLES (MAX_PARTICLES / SPACE_PARTITIONS)

// Partition based collision detection
typedef struct {
    u32 amount;
    u32 points[MAX_PARTITION_PARTICLES];
} Partition;

typedef struct {
    float *speedsX;
    float *speedsY;
    float *positionsX;
    float *positionsY;
    float *radiuses;
    u32 amount;
    u8 *colors;
} Points;
