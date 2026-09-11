#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

//blocks are defined here
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
    BLOCK_GLASS,
    BLOCK_NATURAL_STONE,
    BLOCK_STONE_BRICKS,
    BLOCK_SMOOTH_STONE,
    BLOCK_OBSIDIAN,
    BLOCK_SAND,
    BLOCK_GRAVEL,
    BLOCK_GRASS_PATH,
    BLOCK_ENDSTONE,
    BLOCK_ENDSTONE_BRICKS,
    BLOCK_PURPLE_STAINED_GLASS,
    BLOCK_BLUE_STAINED_GLASS,
    BLOCK_GREEN_STAINED_GLASS,
    BLOCK_RED_STAINED_GLASS,
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
    return type == BLOCK_OAK_LEAVES || type == BLOCK_GLASS ||
           type == BLOCK_PURPLE_STAINED_GLASS ||
           type == BLOCK_BLUE_STAINED_GLASS ||
           type == BLOCK_GREEN_STAINED_GLASS ||
           type == BLOCK_RED_STAINED_GLASS;
}

static inline int block_is_glass(BlockType type) {
    return type == BLOCK_GLASS ||
           type == BLOCK_PURPLE_STAINED_GLASS ||
           type == BLOCK_BLUE_STAINED_GLASS ||
           type == BLOCK_GREEN_STAINED_GLASS ||
           type == BLOCK_RED_STAINED_GLASS;
}

static inline int block_stops_skylight(BlockType type) {
    return block_opaque(type) && !block_is_glass(type);
}

static inline int block_full_cube(BlockType type) {
    return block_opaque(type) && type != BLOCK_GRASS_PATH;
}

static inline int block_falls(BlockType type) {
    return type == BLOCK_SAND || type == BLOCK_GRAVEL;
}

#endif