#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define KB(x) x * 1024
#define MB(x) KB(x) * 1024

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

struct {
    u64 size;
    u64 used;
    u8 *memory;
} typedef BumpAllocator;

void crash() {
    int *x = 0;
    printf("FATAL ERROR\n");
    *x = 2;
}

BumpAllocator *NewBumpAlloc(u64 len) {
    BumpAllocator *ba = (BumpAllocator *)malloc(sizeof(BumpAllocator));

    ba->size = len;
    ba->used = 0;
    ba->memory = (u8 *)malloc(len);

    if (!ba->memory) {
        printf("ERROR: Failed to allocate memory for the bumpAllocator!");
        crash();
        ba->size = 0;
        return 0;
    }

    memset(ba->memory, 0, ba->size);
    return ba;
}

void freeBumpAllocator(BumpAllocator *alloc) {
    memset(alloc->memory, 0, alloc->used + 1);
    alloc->used = 0;
}

u8 *alloc(BumpAllocator *alloc, size_t len) {
    u8 *result = 0;

    // ( l+7 ) & ~7 -> First 3 bits are empty, i.e., it is a multiple of 8.
    size_t allignedSize = (len + 7) & ~7;

    if (alloc->used + allignedSize > alloc->size) {
        printf("Not enough space in BumpAllocator\n");
        printf("Used %lu. New Size: %lu. Capacity: %lu.\n", alloc->used, allignedSize, alloc->size);
        crash();
        return result;
    }

    result = alloc->memory + alloc->used;
    alloc->used += allignedSize;

    return result;
}
