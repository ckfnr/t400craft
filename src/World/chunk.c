#include "chunk.h"
#include <stddef.h>

void chunk_init(Chunk* chunk, int cx, int cz) {
    chunk->x = cx;
    chunk->z = cz;
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y == 0) {
                    chunk->blocks[x][y][z].type = BLOCK_DIRT;
                } else if (y == 1) {
                    chunk->blocks[x][y][z].type = BLOCK_GRASS;
                } else {
                    chunk->blocks[x][y][z].type = BLOCK_AIR;
                }
            }
        }
    }
}

Block* chunk_get_block(Chunk* chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE_X) return NULL;
    if (y < 0 || y >= CHUNK_SIZE_Y) return NULL;
    if (z < 0 || z >= CHUNK_SIZE_Z) return NULL;
    return &chunk->blocks[x][y][z];
}