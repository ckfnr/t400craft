#ifndef CHUNK_H
#define CHUNK_H

#include "block.h"

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 16
#define CHUNK_SIZE_Z 16

typedef struct {
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    int x, z;  // chunk position in world
} Chunk;

void chunk_init(Chunk* chunk, int x, int z);
Block* chunk_get_block(Chunk* chunk, int x, int y, int z);

#endif