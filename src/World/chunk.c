#include "chunk.h"
#include <stddef.h>
#include <math.h>

static float smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t hash2(int x, int z) {
    uint32_t h = (uint32_t)(x * 1619 + z * 31337);
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
}

static float grad(int gx, int gz) {
    uint32_t h = hash2(gx, gz);
    return (float)(h & 0xFFFF) / 65535.0f;
}

static float value_noise(float wx, float wz) {
    int x0 = (int)floorf(wx);
    int z0 = (int)floorf(wz);
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    float fx = smooth(wx - (float)x0);
    float fz = smooth(wz - (float)z0);
    float v00 = grad(x0, z0);
    float v10 = grad(x1, z0);
    float v01 = grad(x0, z1);
    float v11 = grad(x1, z1);
    float vx0 = v00 + fx * (v10 - v00);
    float vx1 = v01 + fx * (v11 - v01);
    return vx0 + fz * (vx1 - vx0);
}

static int terrain_height(int wx, int wz) {
    float nx = (float)wx / 64.0f;
    float nz = (float)wz / 64.0f;
    float h = value_noise(nx, nz) * 0.6f
            + value_noise(nx * 2.0f, nz * 2.0f) * 0.25f
            + value_noise(nx * 4.0f, nz * 4.0f) * 0.10f
            + value_noise(nx * 8.0f, nz * 8.0f) * 0.05f;
    int base = 60;
    int range = 24;
    return base + (int)(h * (float)range);
}

void chunk_generate(Chunk* chunk, int cx, int cz) {
    chunk->cx = cx;
    chunk->cz = cz;
    chunk->dirty = 0;
    int world_x0 = cx * CHUNK_SIZE_X;
    int world_z0 = cz * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int top = terrain_height(world_x0 + x, world_z0 + z);
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockType t;
                if (y > top) {
                    t = BLOCK_AIR;
                } else if (y == top) {
                    t = BLOCK_GRASS;
                } else if (y >= top - 4) {
                    t = BLOCK_DIRT;
                } else {
                    t = BLOCK_STONE;
                }
                chunk->blocks[x][y][z].type = t;
                chunk->blocks[x][y][z].level = 0;
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