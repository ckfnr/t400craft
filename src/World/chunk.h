#ifndef CHUNK_H
#define CHUNK_H

#include "block.h"
#include <stdint.h>

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 16

typedef struct {
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    int cx, cz;
    int dirty;
} Chunk;

void chunk_generate(Chunk* chunk, int cx, int cz);
Block* chunk_get_block(Chunk* chunk, int x, int y, int z);

#endif
