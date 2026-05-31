#ifndef BLOCK_H
#define BLOCK_H

typedef enum {
    BLOCK_AIR = 0,
    BLOCK_DIRT,
} BlockType;

typedef struct {
    BlockType type;
} Block;

#endif