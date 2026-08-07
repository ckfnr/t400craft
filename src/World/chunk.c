#include "chunk.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#define SEA_LEVEL 62

static float smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)(x * 1619 + z * 31337) + seed * 0x9E3779B9u;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
}

static uint32_t hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = (uint32_t)(x * 1619 + y * 52591 + z * 31337) + seed * 0x9E3779B9u;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
}

static float grad(int gx, int gz, uint32_t seed) {
    uint32_t h = hash2(gx, gz, seed);
    return (float)(h & 0xFFFF) / 65535.0f;
}

static float grad3(int gx, int gy, int gz, uint32_t seed) {
    uint32_t h = hash3(gx, gy, gz, seed);
    return (float)(h & 0xFFFF) / 65535.0f;
}

static float value_noise(float wx, float wz, uint32_t seed) {
    int x0 = (int)floorf(wx);
    int z0 = (int)floorf(wz);
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    float fx = smooth(wx - (float)x0);
    float fz = smooth(wz - (float)z0);
    float v00 = grad(x0, z0, seed);
    float v10 = grad(x1, z0, seed);
    float v01 = grad(x0, z1, seed);
    float v11 = grad(x1, z1, seed);
    float vx0 = v00 + fx * (v10 - v00);
    float vx1 = v01 + fx * (v11 - v01);
    return vx0 + fz * (vx1 - vx0);
}

static float value_noise3(float wx, float wy, float wz, uint32_t seed) {
    int x0 = (int)floorf(wx), y0 = (int)floorf(wy), z0 = (int)floorf(wz);
    int x1 = x0 + 1, y1 = y0 + 1, z1 = z0 + 1;
    float fx = smooth(wx - (float)x0);
    float fy = smooth(wy - (float)y0);
    float fz = smooth(wz - (float)z0);
    float v000 = grad3(x0, y0, z0, seed), v100 = grad3(x1, y0, z0, seed);
    float v010 = grad3(x0, y1, z0, seed), v110 = grad3(x1, y1, z0, seed);
    float v001 = grad3(x0, y0, z1, seed), v101 = grad3(x1, y0, z1, seed);
    float v011 = grad3(x0, y1, z1, seed), v111 = grad3(x1, y1, z1, seed);
    float x00 = v000 + fx * (v100 - v000);
    float x10 = v010 + fx * (v110 - v010);
    float x01 = v001 + fx * (v101 - v001);
    float x11 = v011 + fx * (v111 - v011);
    float y0v = x00 + fy * (x10 - x00);
    float y1v = x01 + fy * (x11 - x01);
    return y0v + fz * (y1v - y0v);
}

static int terrain_height(int wx, int wz, uint32_t seed) {
    float nx = (float)wx / 64.0f;
    float nz = (float)wz / 64.0f;
    float h = value_noise(nx, nz, seed) * 0.6f
            + value_noise(nx * 2.0f, nz * 2.0f, seed) * 0.25f
            + value_noise(nx * 4.0f, nz * 4.0f, seed) * 0.10f
            + value_noise(nx * 8.0f, nz * 8.0f, seed) * 0.05f;
    int base = 60;
    int range = 24;
    return base + (int)(h * (float)range);
}

static int natural_height(int wx, int wz, uint32_t seed) {
    float nx = (float)wx / 64.0f;
    float nz = (float)wz / 64.0f;
    float cont = value_noise((float)wx / 320.0f, (float)wz / 320.0f, seed + 101u);
    float h = value_noise(nx, nz, seed) * 0.6f
            + value_noise(nx * 2.0f, nz * 2.0f, seed) * 0.25f
            + value_noise(nx * 4.0f, nz * 4.0f, seed) * 0.10f
            + value_noise(nx * 8.0f, nz * 8.0f, seed) * 0.05f;
    float e = cont * 2.0f - 0.55f;
    if (e < 0.0f) e *= 0.7f;
    int top = SEA_LEVEL + (int)(e * 26.0f + (h - 0.5f) * 20.0f);
    if (top < 8) top = 8;
    if (top > 120) top = 120;
    return top;
}

static int cave_at(int wx, int wy, int wz, uint32_t seed) {
    float n1 = value_noise3((float)wx / 26.0f, (float)wy / 18.0f, (float)wz / 26.0f, seed + 7u);
    float n2 = value_noise3((float)wx / 22.0f, (float)wy / 16.0f, (float)wz / 22.0f, seed + 13u);
    return fabsf(n1 - 0.5f) < 0.045f && fabsf(n2 - 0.5f) < 0.05f;
}

static int tree_at(int wx, int wz, uint32_t seed) {
    if (value_noise((float)wx / 48.0f, (float)wz / 48.0f, seed + 55u) < 0.55f) return 0;
    return hash2(wx, wz, seed + 777u) % 100u < 3u;
}

static void set_block_if_air(Chunk* chunk, int lx, int y, int lz, BlockType type) {
    if (lx < 0 || lx >= CHUNK_SIZE_X) return;
    if (y < 0 || y >= CHUNK_SIZE_Y) return;
    if (lz < 0 || lz >= CHUNK_SIZE_Z) return;
    if (chunk->blocks[lx][y][lz].type != BLOCK_AIR) return;
    chunk->blocks[lx][y][lz].type = type;
    chunk->blocks[lx][y][lz].level = 0;
}

static void place_tree(Chunk* chunk, int world_x0, int world_z0, int tx, int tz, int top, uint32_t seed) {
    int trunk_h = 4 + (int)(hash2(tx, tz, seed + 31u) % 3u);
    int lx = tx - world_x0;
    int lz = tz - world_z0;
    if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z)
        for (int y = top + 1; y <= top + trunk_h && y < CHUNK_SIZE_Y; y++) {
            chunk->blocks[lx][y][lz].type = BLOCK_OAK_LOG;
            chunk->blocks[lx][y][lz].level = 0;
        }
    int leaf_top = top + trunk_h;
    for (int layer = 0; layer < 2; layer++) {
        int y = leaf_top - 1 + layer;
        for (int dx = -2; dx <= 2; dx++)
            for (int dz = -2; dz <= 2; dz++) {
                if (dx == 0 && dz == 0) continue;
                if (abs(dx) == 2 && abs(dz) == 2 && hash3(tx + dx, y, tz + dz, seed + 91u) % 2u == 0u) continue;
                set_block_if_air(chunk, tx + dx - world_x0, y, tz + dz - world_z0, BLOCK_OAK_LEAVES);
            }
    }
    for (int dx = -1; dx <= 1; dx++)
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;
            set_block_if_air(chunk, tx + dx - world_x0, leaf_top + 1, tz + dz - world_z0, BLOCK_OAK_LEAVES);
        }
    set_block_if_air(chunk, tx - world_x0, leaf_top + 1, tz - world_z0, BLOCK_OAK_LEAVES);
    set_block_if_air(chunk, tx - world_x0, leaf_top + 2, tz - world_z0, BLOCK_OAK_LEAVES);
    set_block_if_air(chunk, tx - 1 - world_x0, leaf_top + 2, tz - world_z0, BLOCK_OAK_LEAVES);
    set_block_if_air(chunk, tx + 1 - world_x0, leaf_top + 2, tz - world_z0, BLOCK_OAK_LEAVES);
    set_block_if_air(chunk, tx - world_x0, leaf_top + 2, tz - 1 - world_z0, BLOCK_OAK_LEAVES);
    set_block_if_air(chunk, tx - world_x0, leaf_top + 2, tz + 1 - world_z0, BLOCK_OAK_LEAVES);
}

static void generate_empty(Chunk* chunk, int world_x0, int world_z0, uint32_t seed) {
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int top = terrain_height(world_x0 + x, world_z0 + z, seed);
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

static void generate_natural(Chunk* chunk, int world_x0, int world_z0, uint32_t seed) {
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int wx = world_x0 + x;
            int wz = world_z0 + z;
            int top = natural_height(wx, wz, seed);
            int beach = top <= SEA_LEVEL + 1;
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                BlockType t;
                uint8_t level = 0;
                if (y > top) {
                    if (y <= SEA_LEVEL) {
                        t = BLOCK_WATER;
                        level = WATER_LEVEL_SOURCE;
                    } else {
                        t = BLOCK_AIR;
                    }
                } else if (y == top) {
                    t = beach ? BLOCK_SAND : BLOCK_GRASS;
                } else if (y >= top - 3) {
                    t = beach ? BLOCK_SAND : BLOCK_DIRT;
                } else {
                    t = BLOCK_STONE;
                }
                if (t == BLOCK_STONE && y > 5 && y < top - 5 && cave_at(wx, y, wz, seed))
                    t = BLOCK_AIR;
                chunk->blocks[x][y][z].type = t;
                chunk->blocks[x][y][z].level = level;
            }
        }
    }
    for (int tx = world_x0 - 2; tx <= world_x0 + CHUNK_SIZE_X + 1; tx++)
        for (int tz = world_z0 - 2; tz <= world_z0 + CHUNK_SIZE_Z + 1; tz++) {
            if (!tree_at(tx, tz, seed)) continue;
            int top = natural_height(tx, tz, seed);
            if (top <= SEA_LEVEL + 1 || top + 8 >= CHUNK_SIZE_Y) continue;
            place_tree(chunk, world_x0, world_z0, tx, tz, top, seed);
        }
}

void chunk_generate(Chunk* chunk, int cx, int cz, uint32_t seed, int natural) {
    chunk->cx = cx;
    chunk->cz = cz;
    chunk->dirty = 0;
    int world_x0 = cx * CHUNK_SIZE_X;
    int world_z0 = cz * CHUNK_SIZE_Z;
    if (natural)
        generate_natural(chunk, world_x0, world_z0, seed);
    else
        generate_empty(chunk, world_x0, world_z0, seed);
}

Block* chunk_get_block(Chunk* chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE_X) return NULL;
    if (y < 0 || y >= CHUNK_SIZE_Y) return NULL;
    if (z < 0 || z >= CHUNK_SIZE_Z) return NULL;
    return &chunk->blocks[x][y][z];
}
