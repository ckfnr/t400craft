#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_COBBLESTONE,
    BLOCK_STONE,
    BLOCK_OAK_PLANKS,
    BLOCK_WATER,
    BLOCK_OAK_LOG,
    BLOCK_OAK_LEAVES,
} BlockType;

#define WATER_LEVEL_SOURCE  8
#define WATER_LEVEL_FALLING 9

typedef struct {
    BlockType type;
    uint8_t level;
} Block;

static inline int block_opaque(BlockType type) {
    return type != BLOCK_AIR && type != BLOCK_WATER;
}

static inline int block_transparent(BlockType type) {
    return type == BLOCK_OAK_LEAVES;
}

#endif