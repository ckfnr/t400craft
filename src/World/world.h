#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"
#include "../Mesh/mesh.h"
#include <stdint.h>

#define WORLD_RADIUS 16
#define WORLD_DIAMETER (WORLD_RADIUS * 2 + 1)
#define WORLD_SLOTS (WORLD_DIAMETER * WORLD_DIAMETER)

typedef struct {
    Chunk  chunk;
    Mesh   mesh;
    Mesh   water_mesh;
    Mesh   cutout_mesh;
    int    loaded;
    int    mesh_valid;
    int    mesh_dirty;
    uint8_t lightmap[16][128][16];
    int    lightmap_valid;
} WorldSlot;

typedef struct { int x, y, z; } WaterPos;
typedef struct { int x, y, z; } FallPos;

typedef struct {
    int ix, iz;
    int y0;
    float y;
    float vel_y;
    float delay;
    int started;
    int target_y;
    BlockType type;
} FallingBlock;

typedef struct {
    WorldSlot slots[WORLD_SLOTS];
    int center_cx;
    int center_cz;
    char save_dir[256];
    int dynamic_lighting;
    WaterPos* water_queue;
    int water_count;
    int water_cap;
    int* water_hash;
    int water_hash_cap;
    float water_timer;
    FallPos* fall_queue;
    int fall_count;
    int fall_cap;
    int* fall_hash;
    int fall_hash_cap;
    float fall_timer;
    FallingBlock* falling;
    int falling_count;
    int falling_cap;
} World;

void world_init(World* world, int center_cx, int center_cz, const char* save_dir);
void world_free(World* world);

void world_update_center(World* world, int new_cx, int new_cz);
void world_stream_missing(World* world, int budget);

WorldSlot* world_get_slot(World* world, int cx, int cz);
Chunk*     world_get_chunk(World* world, int cx, int cz);
Block*     world_get_block(World* world, int wx, int wy, int wz);
int        world_set_block(World* world, int wx, int wy, int wz, BlockType type);

int  world_place_water(World* world, int wx, int wy, int wz);
void world_schedule_water(World* world, int wx, int wy, int wz);
void world_update_water(World* world, float dt);

void world_schedule_gravity(World* world, int wx, int wy, int wz);
void world_update_gravity(World* world, float dt);

void world_rebuild_mesh(World* world, int cx, int cz);
void world_save_chunk(World* world, int cx, int cz);
void world_save_all_dirty(World* world);

#endif