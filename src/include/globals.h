#pragma once

#include "memory.h"
#include "types.h"

static BumpAllocator *tempStorage;

static Points pts;

static int PARTITION_SIZE;
static Partition parts[SPACE_PARTITIONS][SPACE_PARTITIONS];

static Vector2 worldSize = {2560, 1440};
static int w, h;
static float dt;
