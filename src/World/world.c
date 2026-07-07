#include "world.h"
#include "chunk_mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

static void slot_index(int cx, int cz, int center_cx, int center_cz, int* out_idx) {
    if (cx < center_cx - WORLD_RADIUS || cx > center_cx + WORLD_RADIUS ||
        cz < center_cz - WORLD_RADIUS || cz > center_cz + WORLD_RADIUS) {
        *out_idx = -1;
        return;
    }
    int lx = ((cx % WORLD_DIAMETER) + WORLD_DIAMETER) % WORLD_DIAMETER;
    int lz = ((cz % WORLD_DIAMETER) + WORLD_DIAMETER) % WORLD_DIAMETER;
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
    long n = (long)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
    int has_levels = 0;
    uint8_t* buf = NULL;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        fseek(f, 0, SEEK_SET);
        has_levels = (fsz >= 2L * n);
        long total = has_levels ? 2L * n : n;
        buf = malloc((size_t)total);
        if (buf) {
            size_t got = fread(buf, 1, (size_t)total, f);
            if (got < (size_t)total)
                memset(buf + got, 0, (size_t)total - got);
        }
        fclose(f);
    }
    if (buf) {
        long i = 0;
        for (int x = 0; x < CHUNK_SIZE_X; x++)
            for (int y = 0; y < CHUNK_SIZE_Y; y++)
                for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                    s->chunk.blocks[x][y][z].type = (BlockType)buf[i++];
                    s->chunk.blocks[x][y][z].level = has_levels ? buf[i++] : 0;
                }
        free(buf);
    } else {
        chunk_generate(&s->chunk, cx, cz);
    }
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
                if (s->chunk.blocks[x][y][z].type == BLOCK_WATER)
                    world_schedule_water(world,
                        cx * CHUNK_SIZE_X + x, y, cz * CHUNK_SIZE_Z + z);
}

static void save_slot_file(World* world, WorldSlot* s) {
    char path[512];
    save_path(world, s->chunk.cx, s->chunk.cz, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (!f) return;
    long n = (long)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
    uint8_t* buf = malloc((size_t)(2L * n));
    if (!buf) {
        fclose(f);
        return;
    }
    long i = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                buf[i++] = (uint8_t)s->chunk.blocks[x][y][z].type;
                buf[i++] = s->chunk.blocks[x][y][z].level;
            }
    fwrite(buf, 1, (size_t)(2L * n), f);
    free(buf);
    fclose(f);
    s->chunk.dirty = 0;
}

static void unload_slot(World* world, int slot_idx) {
    WorldSlot* s = &world->slots[slot_idx];
    if (!s->loaded) return;
    if (s->chunk.dirty)
        save_slot_file(world, s);
    if (s->mesh_valid) {
        mesh_delete(&s->mesh);
        mesh_delete(&s->water_mesh);
        s->mesh_valid = 0;
    }
    s->loaded = 0;
}

static void mark_neighbor_meshes_dirty(World* world, int cx, int cz) {
    static const int ndx[5] = {0, -1, 1, 0, 0};
    static const int ndz[5] = {0, 0, 0, -1, 1};
    for (int i = 0; i < 5; i++) {
        WorldSlot* s = world_get_slot(world, cx + ndx[i], cz + ndz[i]);
        if (s) {
            s->lightmap_valid = 0;
            s->mesh_dirty = 1;
        }
    }
}

static void load_missing_slot(World* world, int idx, int cx, int cz) {
    WorldSlot* s = &world->slots[idx];
    if (s->loaded && s->chunk.cx == cx && s->chunk.cz == cz) return;
    if (s->loaded) unload_slot(world, idx);
    load_or_generate(world, idx, cx, cz);
    mark_neighbor_meshes_dirty(world, cx, cz);
}

void world_init(World* world, int center_cx, int center_cz, const char* save_dir) {
    memset(world, 0, sizeof(World));
    strncpy(world->save_dir, save_dir, sizeof(world->save_dir) - 1);
    world->center_cx = center_cx;
    world->center_cz = center_cz;
    world->dynamic_lighting = 1;

    mkdir(save_dir, 0755);

    for (int cz = center_cz - WORLD_RADIUS; cz <= center_cz + WORLD_RADIUS; cz++) {
        for (int cx = center_cx - WORLD_RADIUS; cx <= center_cx + WORLD_RADIUS; cx++) {
            int idx;
            slot_index(cx, cz, center_cx, center_cz, &idx);
            load_or_generate(world, idx, cx, cz);
        }
    }
}

void world_free(World* world) {
    for (int i = 0; i < WORLD_SLOTS; i++) {
        if (world->slots[i].loaded)
            unload_slot(world, i);
    }
    free(world->water_queue);
    world->water_queue = NULL;
    world->water_count = 0;
    world->water_cap = 0;
}

void world_save_chunk(World* world, int cx, int cz) {
    WorldSlot* s = world_get_slot(world, cx, cz);
    if (!s) return;
    save_slot_file(world, s);
}

void world_save_all_dirty(World* world) {
    for (int i = 0; i < WORLD_SLOTS; i++) {
        if (world->slots[i].loaded && world->slots[i].chunk.dirty)
            world_save_chunk(world, world->slots[i].chunk.cx, world->slots[i].chunk.cz);
    }
}

void world_update_center(World* world, int new_cx, int new_cz) {
    if (new_cx == world->center_cx && new_cz == world->center_cz) return;

    world->center_cx = new_cx;
    world->center_cz = new_cz;

    for (int i = 0; i < WORLD_SLOTS; i++) {
        WorldSlot* s = &world->slots[i];
        if (!s->loaded) continue;
        if (abs(s->chunk.cx - new_cx) > WORLD_RADIUS ||
            abs(s->chunk.cz - new_cz) > WORLD_RADIUS)
            unload_slot(world, i);
    }
}

void world_stream_missing(World* world, int budget) {
    if (budget <= 0) return;
    for (int ring = 0; ring <= WORLD_RADIUS && budget > 0; ring++) {
        int min_cx = world->center_cx - ring;
        int max_cx = world->center_cx + ring;
        int min_cz = world->center_cz - ring;
        int max_cz = world->center_cz + ring;

        for (int cx = min_cx; cx <= max_cx && budget > 0; cx++) {
            for (int cz = min_cz; cz <= max_cz && budget > 0; cz++) {
                if (abs(cx - world->center_cx) != ring && abs(cz - world->center_cz) != ring) continue;
                int idx;
                slot_index(cx, cz, world->center_cx, world->center_cz, &idx);
                if (idx < 0) continue;
                WorldSlot* s = &world->slots[idx];
                if (s->loaded && s->chunk.cx == cx && s->chunk.cz == cz) continue;
                load_missing_slot(world, idx, cx, cz);
                budget--;
            }
        }
    }
}
WorldSlot* world_get_slot(World* world, int cx, int cz) {
    int idx;
    slot_index(cx, cz, world->center_cx, world->center_cz, &idx);
    if (idx < 0 || !world->slots[idx].loaded) return NULL;
    if (world->slots[idx].chunk.cx != cx || world->slots[idx].chunk.cz != cz) return NULL;
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
    b->level = 0;
    c->dirty = 1;
    world_schedule_water(world, wx, wy, wz);
    world_schedule_water(world, wx + 1, wy, wz);
    world_schedule_water(world, wx - 1, wy, wz);
    world_schedule_water(world, wx, wy + 1, wz);
    world_schedule_water(world, wx, wy - 1, wz);
    world_schedule_water(world, wx, wy, wz + 1);
    world_schedule_water(world, wx, wy, wz - 1);
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
    Mesh new_mesh, new_water;
    if (world->dynamic_lighting)
        chunk_build_mesh_with_cached_neighbors(&s->chunk, nchunks,
            nb[0] ? (uint8_t*)nb[0]->lightmap : NULL,
            nb[1] ? (uint8_t*)nb[1]->lightmap : NULL,
            nb[2] ? (uint8_t*)nb[2]->lightmap : NULL,
            nb[3] ? (uint8_t*)nb[3]->lightmap : NULL, 1,
            &new_mesh, &new_water);
    else
        chunk_build_mesh_with_neighbors(&s->chunk, nchunks, 0, &new_mesh, &new_water);
    if (s->mesh_valid) {
        mesh_delete(&s->mesh);
        mesh_delete(&s->water_mesh);
    }
    s->mesh = new_mesh;
    s->water_mesh = new_water;
    s->mesh_valid = 1;
    s->mesh_dirty = 0;
}

void world_schedule_water(World* world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;
    for (int i = 0; i < world->water_count; i++) {
        WaterPos* p = &world->water_queue[i];
        if (p->x == wx && p->y == wy && p->z == wz) return;
    }
    if (world->water_count >= world->water_cap) {
        int nc = world->water_cap ? world->water_cap * 2 : 256;
        WaterPos* nq = realloc(world->water_queue, sizeof(WaterPos) * nc);
        if (!nq) return;
        world->water_queue = nq;
        world->water_cap = nc;
    }
    world->water_queue[world->water_count++] = (WaterPos){wx, wy, wz};
}

static void water_mark_mesh(World* world, int wx, int wz) {
    int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
    int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;
    WorldSlot* s = world_get_slot(world, cx, cz);
    if (s) s->mesh_dirty = 1;
    if (lx == 0)                { s = world_get_slot(world, cx - 1, cz); if (s) s->mesh_dirty = 1; }
    if (lx == CHUNK_SIZE_X - 1) { s = world_get_slot(world, cx + 1, cz); if (s) s->mesh_dirty = 1; }
    if (lz == 0)                { s = world_get_slot(world, cx, cz - 1); if (s) s->mesh_dirty = 1; }
    if (lz == CHUNK_SIZE_Z - 1) { s = world_get_slot(world, cx, cz + 1); if (s) s->mesh_dirty = 1; }
}

static void water_set(World* world, int wx, int wy, int wz, BlockType type, uint8_t level) {
    Block* b = world_get_block(world, wx, wy, wz);
    if (!b) return;
    b->type = type;
    b->level = level;
    int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
    int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
    Chunk* c = world_get_chunk(world, cx, cz);
    if (c) c->dirty = 1;
    water_mark_mesh(world, wx, wz);
    world_schedule_water(world, wx, wy, wz);
    world_schedule_water(world, wx + 1, wy, wz);
    world_schedule_water(world, wx - 1, wy, wz);
    world_schedule_water(world, wx, wy + 1, wz);
    world_schedule_water(world, wx, wy - 1, wz);
    world_schedule_water(world, wx, wy, wz + 1);
    world_schedule_water(world, wx, wy, wz - 1);
}

int world_place_water(World* world, int wx, int wy, int wz) {
    Block* b = world_get_block(world, wx, wy, wz);
    if (!b) return 0;
    if (b->type == BLOCK_WATER) {
        water_set(world, wx, wy, wz, BLOCK_WATER, WATER_LEVEL_SOURCE);
        int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
        int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
        world_rebuild_mesh(world, cx, cz);
        return 1;
    }
    if (block_opaque(b->type)) return 0;
    water_set(world, wx, wy, wz, BLOCK_WATER, WATER_LEVEL_SOURCE);
    int cx = (int)floorf((float)wx / CHUNK_SIZE_X);
    int cz = (int)floorf((float)wz / CHUNK_SIZE_Z);
    world_rebuild_mesh(world, cx, cz);
    return 1;
}

static const int wdx4[4] = {1, -1, 0, 0};
static const int wdz4[4] = {0, 0, 1, -1};

static int water_flow_cost(World* world, int wx, int wy, int wz, int depth, int skip_dir) {
    if (depth > 4) return 1000;
    int best = 1000;
    for (int d = 0; d < 4; d++) {
        if (d == skip_dir) continue;
        int nx = wx + wdx4[d], nz = wz + wdz4[d];
        Block* nb = world_get_block(world, nx, wy, nz);
        if (!nb) continue;
        if (block_opaque(nb->type)) continue;
        if (nb->type == BLOCK_WATER && nb->level == WATER_LEVEL_SOURCE) continue;
        Block* below = world_get_block(world, nx, wy - 1, nz);
        if (below && !block_opaque(below->type)) return depth;
        int c = water_flow_cost(world, nx, wy, nz, depth + 1, d ^ 1);
        if (c < best) best = c;
    }
    return best;
}

static void water_update_cell(World* world, int wx, int wy, int wz) {
    Block* b = world_get_block(world, wx, wy, wz);
    if (!b || b->type != BLOCK_WATER) return;

    Block* above = world_get_block(world, wx, wy + 1, wz);
    Block* below = world_get_block(world, wx, wy - 1, wz);

    if (b->level != WATER_LEVEL_SOURCE) {
        int target = 0;
        if (above && above->type == BLOCK_WATER) {
            target = WATER_LEVEL_FALLING;
        } else {
            for (int d = 0; d < 4; d++) {
                Block* n = world_get_block(world, wx + wdx4[d], wy, wz + wdz4[d]);
                if (!n || n->type != BLOCK_WATER) continue;
                int s = n->level >= WATER_LEVEL_SOURCE ? 8 : n->level;
                if (s - 1 > target) target = s - 1;
            }
        }
        int sources = 0;
        for (int d = 0; d < 4; d++) {
            Block* n = world_get_block(world, wx + wdx4[d], wy, wz + wdz4[d]);
            if (n && n->type == BLOCK_WATER && n->level == WATER_LEVEL_SOURCE) sources++;
        }
        if (sources >= 2 && below &&
            (block_opaque(below->type) ||
             (below->type == BLOCK_WATER && below->level == WATER_LEVEL_SOURCE)))
            target = WATER_LEVEL_SOURCE;
        if (target <= 0) {
            water_set(world, wx, wy, wz, BLOCK_AIR, 0);
            return;
        }
        if ((uint8_t)target != b->level)
            water_set(world, wx, wy, wz, BLOCK_WATER, (uint8_t)target);
    }

    int eff = b->level >= WATER_LEVEL_SOURCE ? 8 : b->level;

    if (below && !block_opaque(below->type)) {
        if (below->type == BLOCK_AIR ||
            (below->type == BLOCK_WATER &&
             below->level != WATER_LEVEL_SOURCE &&
             below->level != WATER_LEVEL_FALLING))
            water_set(world, wx, wy - 1, wz, BLOCK_WATER, WATER_LEVEL_FALLING);
        return;
    }

    if (eff <= 1) return;

    int costs[4];
    int min_cost = 1000;
    for (int d = 0; d < 4; d++) {
        costs[d] = 1000;
        int nx = wx + wdx4[d], nz = wz + wdz4[d];
        Block* n = world_get_block(world, nx, wy, nz);
        if (!n) continue;
        if (block_opaque(n->type)) continue;
        if (n->type == BLOCK_WATER && n->level == WATER_LEVEL_SOURCE) continue;
        Block* nbelow = world_get_block(world, nx, wy - 1, nz);
        if (nbelow && !block_opaque(nbelow->type))
            costs[d] = 0;
        else
            costs[d] = water_flow_cost(world, nx, wy, nz, 1, d ^ 1);
        if (costs[d] < min_cost) min_cost = costs[d];
    }
    for (int d = 0; d < 4; d++) {
        if (costs[d] != min_cost) continue;
        int nx = wx + wdx4[d], nz = wz + wdz4[d];
        Block* n = world_get_block(world, nx, wy, nz);
        if (!n) continue;
        if (n->type == BLOCK_AIR)
            water_set(world, nx, wy, nz, BLOCK_WATER, (uint8_t)(eff - 1));
        else if (n->type == BLOCK_WATER &&
                 n->level != WATER_LEVEL_SOURCE &&
                 n->level != WATER_LEVEL_FALLING &&
                 n->level < eff - 1)
            water_set(world, nx, wy, nz, BLOCK_WATER, (uint8_t)(eff - 1));
    }
}

void world_update_water(World* world, float dt) {
    world->water_timer += dt;
    if (world->water_timer < 0.25f) return;
    world->water_timer = 0.0f;
    int count = world->water_count;
    if (count == 0) return;
    WaterPos* batch = malloc(sizeof(WaterPos) * count);
    if (!batch) return;
    memcpy(batch, world->water_queue, sizeof(WaterPos) * count);
    world->water_count = 0;
    for (int i = 0; i < count; i++)
        water_update_cell(world, batch[i].x, batch[i].y, batch[i].z);
    free(batch);
}