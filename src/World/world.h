#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"
#include "../Mesh/mesh.h"
#include <stdint.h>

#define WORLD_RADIUS 4
#define WORLD_DIAMETER (WORLD_RADIUS * 2 + 1)
#define WORLD_SLOTS (WORLD_DIAMETER * WORLD_DIAMETER)

typedef struct {
    Chunk  chunk;
    Mesh   mesh;
    int    loaded;
    int    mesh_valid;
    int    mesh_dirty;
    uint8_t lightmap[16][128][16];
    int    lightmap_valid;
} WorldSlot;

typedef struct {
    WorldSlot slots[WORLD_SLOTS];
    int center_cx;
    int center_cz;
    char save_dir[256];
    int dynamic_lighting;
} World;

void world_init(World* world, int center_cx, int center_cz, const char* save_dir);
void world_free(World* world);

void world_update_center(World* world, int new_cx, int new_cz);

WorldSlot* world_get_slot(World* world, int cx, int cz);
Chunk*     world_get_chunk(World* world, int cx, int cz);
Block*     world_get_block(World* world, int wx, int wy, int wz);
int        world_set_block(World* world, int wx, int wy, int wz, BlockType type);

void world_rebuild_mesh(World* world, int cx, int cz);
void world_save_chunk(World* world, int cx, int cz);
void world_save_all_dirty(World* world);

#endif