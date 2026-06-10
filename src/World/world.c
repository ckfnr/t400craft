#include "world.h"
#include "chunk_mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

static void slot_index(int cx, int cz, int center_cx, int center_cz, int* out_idx) {
    int lx = cx - (center_cx - WORLD_RADIUS);
    int lz = cz - (center_cz - WORLD_RADIUS);
    if (lx < 0 || lx >= WORLD_DIAMETER || lz < 0 || lz >= WORLD_DIAMETER) {
        *out_idx = -1;
        return;
    }
    *out_idx = lz * WORLD_DIAMETER + lx;
}

static void save_path(World* world, int cx, int cz, char* buf, int bufsz) {
    snprintf(buf, bufsz, "%s/c_%d_%d.bin", world->save_dir, cx, cz);
}

static void load_or_generate(World* world, int slot_idx, int cx, int cz) {
    WorldSlot* s = &world->slots[slot_idx];
    s->chunk.cx = cx;
    s->chunk.cz = cz;
    s->chunk.dirty = 0;
    s->loaded = 1;
    s->mesh_valid = 0;
    s->mesh_dirty = 0;
    s->lightmap_valid = 0;

    char path[512];
    save_path(world, cx, cz, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (f) {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    uint8_t t;
                    if (fread(&t, 1, 1, f) == 1)
                        s->chunk.blocks[x][y][z].type = (BlockType)t;
                }
        fclose(f);
    } else {
        chunk_generate(&s->chunk, cx, cz);
    }
}

static void unload_slot(World* world, int slot_idx) {
    WorldSlot* s = &world->slots[slot_idx];
    if (!s->loaded) return;
    if (s->chunk.dirty)
        world_save_chunk(world, s->chunk.cx, s->chunk.cz);
    if (s->mesh_valid) {
        mesh_delete(&s->mesh);
        s->mesh_valid = 0;
    }
    s->loaded = 0;
}

void world_init(World* world, int center_cx, int center_cz, const char* save_dir) {
    memset(world, 0, sizeof(World));
    strncpy(world->save_dir, save_dir, sizeof(world->save_dir) - 1);
    world->center_cx = center_cx;
    world->center_cz = center_cz;
    world->dynamic_lighting = 1;

    mkdir(save_dir, 0755);

    for (int lz = 0; lz < WORLD_DIAMETER; lz++) {
        for (int lx = 0; lx < WORLD_DIAMETER; lx++) {
            int cx = (center_cx - WORLD_RADIUS) + lx;
            int cz = (center_cz - WORLD_RADIUS) + lz;
            int idx = lz * WORLD_DIAMETER + lx;
            load_or_generate(world, idx, cx, cz);
        }
    }
}

void world_free(World* world) {
    for (int i = 0; i < WORLD_SLOTS; i++) {
        if (world->slots[i].loaded)
            unload_slot(world, i);
    }
}

void world_save_chunk(World* world, int cx, int cz) {
    int idx;
    slot_index(cx, cz, world->center_cx, world->center_cz, &idx);
    if (idx < 0 || !world->slots[idx].loaded) return;
    WorldSlot* s = &world->slots[idx];
    char path[512];
    save_path(world, cx, cz, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (!f) return;
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                uint8_t t = (uint8_t)s->chunk.blocks[x][y][z].type;
                fwrite(&t, 1, 1, f);
            }
    fclose(f);
    s->chunk.dirty = 0;
}

void world_save_all_dirty(World* world) {
    for (int i = 0; i < WORLD_SLOTS; i++) {
        if (world->slots[i].loaded && world->slots[i].chunk.dirty)
            world_save_chunk(world, world->slots[i].chunk.cx, world->slots[i].chunk.cz);
    }
}

void world_update_center(World* world, int new_cx, int new_cz) {
    if (new_cx == world->center_cx && new_cz == world->center_cz) return;

    int old_cx = world->center_cx;
    int old_cz = world->center_cz;

    WorldSlot* old_slots = malloc(sizeof(WorldSlot) * WORLD_SLOTS);
    if (!old_slots) return;
    memcpy(old_slots, world->slots, sizeof(WorldSlot) * WORLD_SLOTS);

    world->center_cx = new_cx;
    world->center_cz = new_cz;

    for (int lz = 0; lz < WORLD_DIAMETER; lz++) {
        for (int lx = 0; lx < WORLD_DIAMETER; lx++) {
            int idx = lz * WORLD_DIAMETER + lx;
            int cx = (new_cx - WORLD_RADIUS) + lx;
            int cz = (new_cz - WORLD_RADIUS) + lz;

            int old_lx = cx - (old_cx - WORLD_RADIUS);
            int old_lz = cz - (old_cz - WORLD_RADIUS);
            int old_idx = -1;
            if (old_lx >= 0 && old_lx < WORLD_DIAMETER && old_lz >= 0 && old_lz < WORLD_DIAMETER)
                old_idx = old_lz * WORLD_DIAMETER + old_lx;

            if (old_idx >= 0 && old_slots[old_idx].loaded
                && old_slots[old_idx].chunk.cx == cx
                && old_slots[old_idx].chunk.cz == cz) {
                world->slots[idx] = old_slots[old_idx];
                old_slots[old_idx].loaded = 0;
                old_slots[old_idx].mesh_valid = 0;
            } else {
                world->slots[idx].loaded = 0;
                world->slots[idx].mesh_valid = 0;
            }
        }
    }

    for (int i = 0; i < WORLD_SLOTS; i++) {
        if (old_slots[i].loaded) {
            if (old_slots[i].chunk.dirty) {
                int cx = old_slots[i].chunk.cx;
                int cz = old_slots[i].chunk.cz;
                char path[512];
                save_path(world, cx, cz, path, sizeof(path));
                FILE* f = fopen(path, "wb");
                if (f) {
                    Chunk* c = &old_slots[i].chunk;
                    for (int x = 0; x < CHUNK_SIZE_X; x++)
                        for (int y = 0; y < CHUNK_SIZE_Y; y++)
                            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                                uint8_t t = (uint8_t)c->blocks[x][y][z].type;
                                fwrite(&t, 1, 1, f);
                            }
                    fclose(f);
                }
            }
            if (old_slots[i].mesh_valid)
                mesh_delete(&old_slots[i].mesh);
        }
    }

    uint8_t newly[WORLD_SLOTS];
    memset(newly, 0, sizeof(newly));
    for (int lz = 0; lz < WORLD_DIAMETER; lz++) {
        for (int lx = 0; lx < WORLD_DIAMETER; lx++) {
            int idx = lz * WORLD_DIAMETER + lx;
            if (!world->slots[idx].loaded) {
                int cx = (new_cx - WORLD_RADIUS) + lx;
                int cz = (new_cz - WORLD_RADIUS) + lz;
                load_or_generate(world, idx, cx, cz);
                newly[idx] = 1;
            }
        }
    }
    for (int lz = 0; lz < WORLD_DIAMETER; lz++) {
        for (int lx = 0; lx < WORLD_DIAMETER; lx++) {
            int idx = lz * WORLD_DIAMETER + lx;
            if (!newly[idx]) continue;
            static const int ndx[4] = {-1, 1, 0, 0};
            static const int ndz[4] = {0, 0, -1, 1};
            for (int d = 0; d < 4; d++) {
                int nlx = lx + ndx[d], nlz = lz + ndz[d];
                if (nlx < 0 || nlx >= WORLD_DIAMETER || nlz < 0 || nlz >= WORLD_DIAMETER) continue;
                int nidx = nlz * WORLD_DIAMETER + nlx;
                if (world->slots[nidx].loaded && !newly[nidx] && world->slots[nidx].mesh_valid)
                    world->slots[nidx].mesh_dirty = 1;
            }
        }
    }
    free(old_slots);
}
WorldSlot* world_get_slot(World* world, int cx, int cz) {
    int idx;
    slot_index(cx, cz, world->center_cx, world->center_cz, &idx);
    if (idx < 0 || !world->slots[idx].loaded) return NULL;
    return &world->slots[idx];
}

Chunk* world_get_chunk(World* world, int cx, int cz) {
    WorldSlot* s = world_get_slot(world, cx, cz);
    return s ? &s->chunk : NULL;
}

Block* world_get_block(World* world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return NULL;
    int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
    int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;
    Chunk* c = world_get_chunk(world, cx, cz);
    if (!c) return NULL;
    return chunk_get_block(c, lx, wy, lz);
}

int world_set_block(World* world, int wx, int wy, int wz, BlockType type) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return 0;
    int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
    int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;
    Chunk* c = world_get_chunk(world, cx, cz);
    if (!c) return 0;
    Block* b = chunk_get_block(c, lx, wy, lz);
    if (!b) return 0;
    b->type = type;
    c->dirty = 1;
    static const int idx2[5] = {0, -1, 1, 0, 0};
    static const int idz2[5] = {0, 0, 0, -1, 1};
    for (int i = 0; i < 5; i++) {
        WorldSlot* s = world_get_slot(world, cx + idx2[i], cz + idz2[i]);
        if (s) {
            s->lightmap_valid = 0;
            s->mesh_dirty = 1;
        }
    }
    world_rebuild_mesh(world, cx, cz);
    return 1;
}

void world_rebuild_mesh(World* world, int cx, int cz) {
    WorldSlot* s = world_get_slot(world, cx, cz);
    if (!s) return;
    WorldSlot* nb[4] = {
        world_get_slot(world, cx - 1, cz),
        world_get_slot(world, cx + 1, cz),
        world_get_slot(world, cx,     cz - 1),
        world_get_slot(world, cx,     cz + 1),
    };
    Chunk* nchunks[4] = {
        nb[0] ? &nb[0]->chunk : NULL,
        nb[1] ? &nb[1]->chunk : NULL,
        nb[2] ? &nb[2]->chunk : NULL,
        nb[3] ? &nb[3]->chunk : NULL,
    };
    if (world->dynamic_lighting) {
        if (!s->lightmap_valid) {
            chunk_compute_lightmap(&s->chunk, s->lightmap);
            s->lightmap_valid = 1;
        }
        for (int i = 0; i < 4; i++) {
            if (nb[i] && !nb[i]->lightmap_valid) {
                chunk_compute_lightmap(&nb[i]->chunk, nb[i]->lightmap);
                nb[i]->lightmap_valid = 1;
            }
        }
    }
    Mesh new_mesh = world->dynamic_lighting
        ? chunk_build_mesh_with_cached_neighbors(&s->chunk, nchunks,
            nb[0] ? (uint8_t*)nb[0]->lightmap : NULL,
            nb[1] ? (uint8_t*)nb[1]->lightmap : NULL,
            nb[2] ? (uint8_t*)nb[2]->lightmap : NULL,
            nb[3] ? (uint8_t*)nb[3]->lightmap : NULL, 1)
        : chunk_build_mesh_with_neighbors(&s->chunk, nchunks, 0);
    if (s->mesh_valid) mesh_delete(&s->mesh);
    s->mesh = new_mesh;
    s->mesh_valid = 1;
    s->mesh_dirty = 0;
}