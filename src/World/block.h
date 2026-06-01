#ifndef BLOCK_H
#define BLOCK_H

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_COBBLESTONE,
} BlockType;

typedef struct {
    BlockType type;
} Block;

#endif