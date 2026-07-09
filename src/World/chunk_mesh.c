#include "chunk_mesh.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VERTICES (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 4)
#define MAX_INDICES  (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 6)
#define MAX_LIGHT 15


// returns the texture layer index for a given block type and face
static int block_face_texture_layer(BlockType type, int face) {
    switch (type) {
        case BLOCK_DIRT:        return 0;
        case BLOCK_GRASS:
            if (face == 0) return 2;
            if (face == 1) return 0;
            return 1;
        case BLOCK_COBBLESTONE: return 3;
        case BLOCK_STONE:       return 3;
        case BLOCK_OAK_PLANKS:  return 4;
        case BLOCK_WATER:       return 5;
        case BLOCK_OAK_LOG:
            if (face == 0 || face == 1) return 7;
            return 6;
        case BLOCK_OAK_LEAVES:  return 8;
        case BLOCK_GLASS:       return 9;
        case BLOCK_NATURAL_STONE: return 10;
        case BLOCK_STONE_BRICKS: return 11;
        case BLOCK_SMOOTH_STONE: return 12;
        case BLOCK_OBSIDIAN:     return 13;
        case BLOCK_SAND:         return 14;
        case BLOCK_GRAVEL:       return 15;
        case BLOCK_AIR:
            return 0;
        case BLOCK_GRASS_PATH:
            if (face == 0) return 17;
            if (face == 1) return 0;
            return 16;
        default:                return 0;
    }
}

typedef struct { short x, y, z; } LightNode;

static uint8_t light_solid(BlockType type) {
    return (uint8_t)(block_opaque(type) && !block_transparent(type));
}

#define EXT_X (CHUNK_SIZE_X + 2)
#define EXT_Z (CHUNK_SIZE_Z + 2)
#define EXT_QCAP (EXT_X * CHUNK_SIZE_Y * EXT_Z + 1)

static uint8_t ext_light[EXT_X][CHUNK_SIZE_Y][EXT_Z];
static uint8_t ext_solid[EXT_X][CHUNK_SIZE_Y][EXT_Z];
static uint8_t ext_in_queue[EXT_X][CHUNK_SIZE_Y][EXT_Z];
static LightNode ext_queue[EXT_QCAP];

void chunk_compute_lightmap(Chunk* chunk,
                             uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z]) {
    memset(light, 0, CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z);
    static LightNode local_queue[CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 2];
    int head = 0, tail = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (block_stops_skylight(chunk->blocks[x][y][z].type)) break;
                light[x][y][z] = MAX_LIGHT;
                local_queue[tail++] = (LightNode){(short)x, (short)y, (short)z};
            }
        }
    }
    static const int dx[] = {1,-1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1,-1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1,-1};
    while (head < tail) {
        LightNode n = local_queue[head++];
        int lv = light[n.x][n.y][n.z];
        if (lv <= 1) continue;
        for (int d = 0; d < 6; d++) {
            int nx = n.x + dx[d], ny = n.y + dy[d], nz = n.z + dz[d];
            if (nx < 0 || nx >= CHUNK_SIZE_X) continue;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
            if (nz < 0 || nz >= CHUNK_SIZE_Z) continue;
            if (light_solid(chunk->blocks[nx][ny][nz].type)) continue;
            if (light[nx][ny][nz] >= lv - 1) continue;
            light[nx][ny][nz] = (uint8_t)(lv - 1);
            local_queue[tail++] = (LightNode){(short)nx, (short)ny, (short)nz};
        }
    }
}

static void build_lightmap_from_cache(Chunk* chunk, Chunk* neighbors[4],
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3,
    uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z]) {

    typedef uint8_t NbMap[CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    NbMap* nbm[4] = {(NbMap*)nb0, (NbMap*)nb1, (NbMap*)nb2, (NbMap*)nb3};
    Chunk* null_nb[4] = {NULL, NULL, NULL, NULL};
    Chunk** nbc = neighbors ? neighbors : null_nb;

    memset(ext_light, 0, sizeof(ext_light));
    memset(ext_solid, 1, sizeof(ext_solid));
    memset(ext_in_queue, 0, sizeof(ext_in_queue));

    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
                ext_solid[x+1][y][z+1] = light_solid(chunk->blocks[x][y][z].type);

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            if (nbc[0]) ext_solid[0][y][z+1]       = light_solid(nbc[0]->blocks[CHUNK_SIZE_X-1][y][z].type);
            if (nbc[1]) ext_solid[EXT_X-1][y][z+1] = light_solid(nbc[1]->blocks[0][y][z].type);
        }
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            if (nbc[2]) ext_solid[x+1][y][0]       = light_solid(nbc[2]->blocks[x][y][CHUNK_SIZE_Z-1].type);
            if (nbc[3]) ext_solid[x+1][y][EXT_Z-1] = light_solid(nbc[3]->blocks[x][y][0].type);
        }
    }

    int head = 0, tail = 0;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (block_stops_skylight(chunk->blocks[x][y][z].type)) break;
                ext_light[x+1][y][z+1] = MAX_LIGHT;
                ext_queue[tail] = (LightNode){(short)(x+1), (short)y, (short)(z+1)};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[x+1][y][z+1] = 1;
            }
        }
    }

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            uint8_t v;
            if (nbm[0] && !ext_solid[0][y][z+1] && (v = nbm[0][CHUNK_SIZE_X-1][y][z]) > 0) {
                ext_light[0][y][z+1] = v;
                ext_queue[tail] = (LightNode){0, (short)y, (short)(z+1)};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[0][y][z+1] = 1;
            }
            if (nbm[1] && !ext_solid[EXT_X-1][y][z+1] && (v = nbm[1][0][y][z]) > 0) {
                ext_light[EXT_X-1][y][z+1] = v;
                ext_queue[tail] = (LightNode){EXT_X-1, (short)y, (short)(z+1)};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[EXT_X-1][y][z+1] = 1;
            }
        }
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            uint8_t v;
            if (nbm[2] && !ext_solid[x+1][y][0] && (v = nbm[2][x][y][CHUNK_SIZE_Z-1]) > 0) {
                ext_light[x+1][y][0] = v;
                ext_queue[tail] = (LightNode){(short)(x+1), (short)y, 0};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[x+1][y][0] = 1;
            }
            if (nbm[3] && !ext_solid[x+1][y][EXT_Z-1] && (v = nbm[3][x][y][0]) > 0) {
                ext_light[x+1][y][EXT_Z-1] = v;
                ext_queue[tail] = (LightNode){(short)(x+1), (short)y, EXT_Z-1};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[x+1][y][EXT_Z-1] = 1;
            }
        }
    }

    static const int dx[] = {1,-1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1,-1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1,-1};
    while (head != tail) {
        LightNode n = ext_queue[head];
        head = (head + 1) % EXT_QCAP;
        ext_in_queue[n.x][n.y][n.z] = 0;
        int lv = ext_light[n.x][n.y][n.z];
        if (lv <= 1) continue;
        for (int d = 0; d < 6; d++) {
            int nx = n.x + dx[d], ny = n.y + dy[d], nz = n.z + dz[d];
            if (nx < 0 || nx >= EXT_X) continue;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
            if (nz < 0 || nz >= EXT_Z) continue;
            if (ext_solid[nx][ny][nz]) continue;
            if (ext_light[nx][ny][nz] >= lv - 1) continue;
            ext_light[nx][ny][nz] = (uint8_t)(lv - 1);
            if (!ext_in_queue[nx][ny][nz]) {
                ext_queue[tail] = (LightNode){(short)nx, (short)ny, (short)nz};
                tail = (tail + 1) % EXT_QCAP;
                ext_in_queue[nx][ny][nz] = 1;
            }
        }
    }

    for (int x = 0; x < CHUNK_SIZE_X; x++)
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
            for (int z = 0; z < CHUNK_SIZE_Z; z++)
                light[x][y][z] = ext_light[x+1][y][z+1];
}

static int directional_lighting_enabled = 0;
static float shadow_step_x = 0.0f;
static float shadow_step_z = 0.0f;

void chunk_mesh_set_directional_lighting(int enabled) {
    directional_lighting_enabled = enabled ? 1 : 0;
}

void chunk_mesh_set_shadow_dir(float step_x, float step_z) {
    shadow_step_x = step_x;
    shadow_step_z = step_z;
}

static void add_face(GLfloat* verts, GLuint* inds, int* vc, int* ic,
                     float x, float y, float z, int face, BlockType type, const float* cb, const float* ch) {
    static const float face_dim[] = {1.0f, 0.6f, 0.8f, 0.8f, 0.7f, 0.7f};
    float dim = directional_lighting_enabled ? 1.0f : face_dim[face];
    float layer = (float)block_face_texture_layer(type, face);
    float top = (type == BLOCK_GRASS_PATH) ? (15.0f / 16.0f) : 1.0f;
    float positions[4][3];
    switch (face) {
        case 0:
            positions[0][0]=x;   positions[0][1]=y+top; positions[0][2]=z+1;
            positions[1][0]=x+1; positions[1][1]=y+top; positions[1][2]=z+1;
            positions[2][0]=x+1; positions[2][1]=y+top; positions[2][2]=z;
            positions[3][0]=x;   positions[3][1]=y+top; positions[3][2]=z;
            break;
        case 1:
            positions[0][0]=x;   positions[0][1]=y; positions[0][2]=z;
            positions[1][0]=x+1; positions[1][1]=y; positions[1][2]=z;
            positions[2][0]=x+1; positions[2][1]=y; positions[2][2]=z+1;
            positions[3][0]=x;   positions[3][1]=y; positions[3][2]=z+1;
            break;
        case 2:
            positions[0][0]=x;   positions[0][1]=y;   positions[0][2]=z+1;
            positions[1][0]=x+1; positions[1][1]=y;   positions[1][2]=z+1;
            positions[2][0]=x+1; positions[2][1]=y+top; positions[2][2]=z+1;
            positions[3][0]=x;   positions[3][1]=y+top; positions[3][2]=z+1;
            break;
        case 3:
            positions[0][0]=x+1; positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x;   positions[1][1]=y;   positions[1][2]=z;
            positions[2][0]=x;   positions[2][1]=y+top; positions[2][2]=z;
            positions[3][0]=x+1; positions[3][1]=y+top; positions[3][2]=z;
            break;
        case 4:
            positions[0][0]=x; positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x; positions[1][1]=y;   positions[1][2]=z+1;
            positions[2][0]=x; positions[2][1]=y+top; positions[2][2]=z+1;
            positions[3][0]=x; positions[3][1]=y+top; positions[3][2]=z;
            break;
        default:
            positions[0][0]=x+1; positions[0][1]=y;   positions[0][2]=z+1;
            positions[1][0]=x+1; positions[1][1]=y;   positions[1][2]=z;
            positions[2][0]=x+1; positions[2][1]=y+top; positions[2][2]=z;
            positions[3][0]=x+1; positions[3][1]=y+top; positions[3][2]=z+1;
            break;
    }
    if (ch) {
        for (int i = 0; i < 4; i++) {
            if (positions[i][1] > y + 0.5f) {
                int xi = positions[i][0] > x + 0.5f;
                int zi = positions[i][2] > z + 0.5f;
                positions[i][1] = y + ch[xi * 2 + zi] * top;
            }
        }
    }
    float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    if (face >= 2) {
        float flipped[4][2] = {{0,1},{1,1},{1,0},{0,0}};
        for (int i = 0; i < 4; ++i) { uvs[i][0]=flipped[i][0]; uvs[i][1]=flipped[i][1]; }
        if (top < 1.0f) {
            float crop = 1.0f - top;
            for (int i = 0; i < 4; ++i)
                if (uvs[i][1] < 0.5f) uvs[i][1] = crop;
        }
    }
    int base = *vc;
    for (int i = 0; i < 4; i++) {
        float b = cb[i] * dim;
        verts[(*vc)*9+0] = positions[i][0];
        verts[(*vc)*9+1] = positions[i][1];
        verts[(*vc)*9+2] = positions[i][2];
        verts[(*vc)*9+3] = b; verts[(*vc)*9+4] = b; verts[(*vc)*9+5] = b;
        verts[(*vc)*9+6] = uvs[i][0];
        verts[(*vc)*9+7] = uvs[i][1];
        verts[(*vc)*9+8] = layer;
        (*vc)++;
    }
    inds[(*ic)++] = base+0; inds[(*ic)++] = base+1; inds[(*ic)++] = base+2;
    inds[(*ic)++] = base+0; inds[(*ic)++] = base+2; inds[(*ic)++] = base+3;
}

static Block* mesh_cell(Chunk* chunk, Chunk* neighbors[4], int nx, int ny, int nz) {
    if (ny < 0 || ny >= CHUNK_SIZE_Y) return NULL;
    int xin = (nx >= 0 && nx < CHUNK_SIZE_X);
    int zin = (nz >= 0 && nz < CHUNK_SIZE_Z);
    if (xin && zin) return &chunk->blocks[nx][ny][nz];
    if (!neighbors || (!xin && !zin)) return NULL;
    if (nx < 0)             return (neighbors[0] && nx >= -CHUNK_SIZE_X)     ? &neighbors[0]->blocks[nx + CHUNK_SIZE_X][ny][nz] : NULL;
    if (nx >= CHUNK_SIZE_X) return (neighbors[1] && nx < 2 * CHUNK_SIZE_X)   ? &neighbors[1]->blocks[nx - CHUNK_SIZE_X][ny][nz] : NULL;
    if (nz < 0)             return (neighbors[2] && nz >= -CHUNK_SIZE_Z)     ? &neighbors[2]->blocks[nx][ny][nz + CHUNK_SIZE_Z] : NULL;
    return                         (neighbors[3] && nz < 2 * CHUNK_SIZE_Z)   ? &neighbors[3]->blocks[nx][ny][nz - CHUNK_SIZE_Z] : NULL;
}

static int mesh_cell_opaque(Chunk* chunk, Chunk* neighbors[4], int nx, int ny, int nz) {
    Block* b = mesh_cell(chunk, neighbors, nx, ny, nz);
    return b && block_opaque(b->type) && b->type != BLOCK_GLASS;
}

static int soft_lighting_enabled = 1;

void chunk_mesh_set_soft_lighting(int enabled) {
    soft_lighting_enabled = enabled ? 1 : 0;
}

static const int face_axes[6][6] = {
    {1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 0},
    {1, 0, 0, 0, 1, 0},
    {0, 1, 0, 0, 0, 1},
    {0, 1, 0, 0, 0, 1},
};

static const int vert_corner_signs[6][4][2] = {
    {{-1, 1}, { 1, 1}, { 1,-1}, {-1,-1}},
    {{-1,-1}, { 1,-1}, { 1, 1}, {-1, 1}},
    {{-1,-1}, { 1,-1}, { 1, 1}, {-1, 1}},
    {{ 1,-1}, {-1,-1}, {-1, 1}, { 1, 1}},
    {{-1,-1}, {-1, 1}, { 1, 1}, { 1,-1}},
    {{-1, 1}, {-1,-1}, { 1,-1}, { 1, 1}},
};

static const float light_table[16] = {
    0.07f, 0.09f, 0.12f, 0.15f, 0.19f, 0.24f, 0.30f, 0.37f,
    0.45f, 0.54f, 0.63f, 0.72f, 0.81f, 0.89f, 0.95f, 1.00f
};

static int ext_light_at(int x, int y, int z) {
    if (y >= CHUNK_SIZE_Y) return MAX_LIGHT;
    if (y < 0) return -1;
    if (x < -1 || x > CHUNK_SIZE_X || z < -1 || z > CHUNK_SIZE_Z) return -1;
    if (ext_solid[x+1][y][z+1]) return -1;
    return ext_light[x+1][y][z+1];
}

static const int face_normals[6][3] = {
    { 0, 1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0}, { 1, 0, 0},
};

static void face_shadow_corners(Chunk* chunk, Chunk* neighbors[4], int x, int y, int z, int face, float out[4]) {
    if (face == 0 && !directional_lighting_enabled) {
        out[0] = out[1] = out[2] = out[3] = 1.0f;
        return;
    }
    const int* axis = face_axes[face];
    int ux = axis[0], uy = axis[1], uz = axis[2];
    int vx = axis[3], vy = axis[4], vz = axis[5];
    int bx = x, by = y, bz = z;
    if (directional_lighting_enabled) {
        bx += face_normals[face][0];
        by += face_normals[face][1];
        bz += face_normals[face][2];
    }

    for (int i = 0; i < 4; i++) {
        int su = vert_corner_signs[face][i][0];
        int sv = vert_corner_signs[face][i][1];
        int side_u = mesh_cell_opaque(chunk, neighbors, bx + su * ux, by + su * uy, bz + su * uz);
        int side_v = mesh_cell_opaque(chunk, neighbors, bx + sv * vx, by + sv * vy, bz + sv * vz);
        int corner = mesh_cell_opaque(chunk, neighbors,
            bx + su * ux + sv * vx,
            by + su * uy + sv * vy,
            bz + su * uz + sv * vz);

        float shade = 1.0f;
        shade -= 0.16f * (float)(side_u + side_v);
        shade -= 0.10f * (float)corner;
        if (side_u && side_v) shade -= 0.10f;
        if (shade < 0.42f) shade = 0.42f;
        out[i] = shade;
    }
}

static float canopy_shadow_factor(Chunk* chunk, Chunk* neighbors[4], int x, int y, int z) {
    float occlusion = 0.0f;
    for (int dy = 1; dy <= 8 && y + dy < CHUNK_SIZE_Y; dy++) {
        int ox = 0, oz = 0;
        if (directional_lighting_enabled) {
            ox = (int)floorf(shadow_step_x * (float)dy + 0.5f);
            oz = (int)floorf(shadow_step_z * (float)dy + 0.5f);
        }
        float height_weight = 1.0f / (float)dy;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                Block* b = mesh_cell(chunk, neighbors, x + ox + dx, y + dy, z + oz + dz);
                if (!b || !block_opaque(b->type)) continue;
                float footprint = b->type == BLOCK_GLASS ? 0.1f : 1.0f;
                if (dx != 0) footprint *= 0.65f;
                if (dz != 0) footprint *= 0.65f;
                occlusion += height_weight * footprint;
            }
        }
    }

    float shadow = 1.0f - occlusion * 0.18f;
    if (shadow < 0.42f) shadow = 0.42f;
    if (shadow > 1.0f) shadow = 1.0f;
    return shadow;
}

static float water_surface_h(uint8_t lvl) {
    if (lvl >= WATER_LEVEL_FALLING) return 1.0f;
    if (lvl == WATER_LEVEL_SOURCE)  return 0.875f;
    return 0.875f * (float)lvl / 8.0f;
}

static float water_corner_h(Chunk* chunk, Chunk* neighbors[4], int x, int y, int z, int xi, int zi) {
    float total = 0.0f, weight = 0.0f;
    for (int ox = -1; ox <= 0; ox++) {
        for (int oz = -1; oz <= 0; oz++) {
            int sx = x + xi + ox, sz = z + zi + oz;
            Block* b = mesh_cell(chunk, neighbors, sx, y, sz);
            if (!b) continue;
            if (b->type == BLOCK_WATER) {
                Block* up = mesh_cell(chunk, neighbors, sx, y + 1, sz);
                if (up && up->type == BLOCK_WATER) return 1.0f;
                float h = water_surface_h(b->level);
                float w = h >= 0.8f ? 10.0f : 1.0f;
                total += h * w;
                weight += w;
            } else if (!block_opaque(b->type)) {
                weight += 1.0f;
            }
        }
    }
    return weight > 0.0f ? total / weight : 1.0f;
}

static void build_mesh(Chunk* chunk, Chunk* neighbors[4],
                        uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z],
                        int dynamic, Mesh* out_solid, Mesh* out_water, Mesh* out_cutout) {
    static const int dx[] = {0, 0, 0, 0, -1, 1};
    static const int dy[] = {1,-1, 0, 0,  0, 0};
    static const int dz[] = {0, 0, 1,-1,  0, 0};

    static GLfloat verts[MAX_VERTICES * 9];
    static GLuint  inds[MAX_INDICES];
    static GLfloat wverts[MAX_VERTICES * 9];
    static GLuint  winds[MAX_INDICES];
    static GLfloat cverts[MAX_VERTICES * 9];
    static GLuint  cinds[MAX_INDICES];
    int vc = 0, ic = 0, wvc = 0, wic = 0, cvc = 0, cic = 0;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                BlockType type = chunk->blocks[x][y][z].type;
                if (type == BLOCK_AIR) continue;
                int is_water = (type == BLOCK_WATER);
                float wch[4];
                if (is_water)
                    for (int xi = 0; xi < 2; xi++)
                        for (int zi = 0; zi < 2; zi++)
                            wch[xi * 2 + zi] = water_corner_h(chunk, neighbors, x, y, z, xi, zi);
                for (int face = 0; face < 6; face++) {
                    int nx = x + dx[face];
                    int ny = y + dy[face];
                    int nz = z + dz[face];
                    int out_of_bounds = (nx < 0 || nx >= CHUNK_SIZE_X ||
                                        ny < 0 || ny >= CHUNK_SIZE_Y ||
                                        nz < 0 || nz >= CHUNK_SIZE_Z);
                    BlockType ntype = BLOCK_AIR;
                    int have_n = 0;
                    if (!out_of_bounds) {
                        ntype = chunk->blocks[nx][ny][nz].type;
                        have_n = 1;
                    } else if (!(ny < 0 || ny >= CHUNK_SIZE_Y)) {
                        Chunk* nbr = NULL;
                        int lx = nx, lnz = nz;
                        if      (nx < 0)             { nbr = neighbors ? neighbors[0] : NULL; lx  = CHUNK_SIZE_X-1; }
                        else if (nx >= CHUNK_SIZE_X) { nbr = neighbors ? neighbors[1] : NULL; lx  = 0; }
                        else if (nz < 0)             { nbr = neighbors ? neighbors[2] : NULL; lnz = CHUNK_SIZE_Z-1; }
                        else                         { nbr = neighbors ? neighbors[3] : NULL; lnz = 0; }
                        if (nbr) {
                            Block* nbb = chunk_get_block(nbr, lx, ny, lnz);
                            if (nbb) { ntype = nbb->type; have_n = 1; }
                        }
                    }
                    if (is_water) {
                        if (have_n && ntype != BLOCK_AIR && !block_transparent(ntype)) continue;
                    } else {
                        if (have_n && block_full_cube(ntype) && !block_transparent(ntype)) continue;
                        if (have_n && type == BLOCK_GLASS && ntype == BLOCK_GLASS) continue;
                        if (have_n && type == BLOCK_OAK_LEAVES && ntype == BLOCK_OAK_LEAVES) continue;
                    }
                    float lv;
                    if (!dynamic) {
                        lv = 1.0f;
                    } else if (!out_of_bounds) {
                        lv = light_table[light[nx][ny][nz]];
                    } else if (ny < 0 || ny >= CHUNK_SIZE_Y) {
                        lv = 1.0f;
                    } else if (nx < 0) {
                        lv = light_table[ext_light[0][ny][nz+1]];
                    } else if (nx >= CHUNK_SIZE_X) {
                        lv = light_table[ext_light[EXT_X-1][ny][nz+1]];
                    } else if (nz < 0) {
                        lv = light_table[ext_light[nx+1][ny][0]];
                    } else {
                        lv = light_table[ext_light[nx+1][ny][EXT_Z-1]];
                    }
                    float shades[4];
                    face_shadow_corners(chunk, neighbors, x, y, z, face, shades);
                    float canopy = 1.0f;
                    if (face == 0)
                        canopy = canopy_shadow_factor(chunk, neighbors, x, y, z);
                    float corner_b[4];
                    if (soft_lighting_enabled && dynamic) {
                        const int* axis = face_axes[face];
                        for (int i = 0; i < 4; i++) {
                            int su = vert_corner_signs[face][i][0];
                            int sv = vert_corner_signs[face][i][1];
                            int sum = 0, cnt = 0;
                            for (int c = 0; c < 4; c++) {
                                int cu = (c & 1) ? su : 0;
                                int cv = (c & 2) ? sv : 0;
                                int v = ext_light_at(nx + cu * axis[0] + cv * axis[3],
                                                     ny + cu * axis[1] + cv * axis[4],
                                                     nz + cu * axis[2] + cv * axis[5]);
                                if (v >= 0) { sum += v; cnt++; }
                            }
                            float cl = cnt ? light_table[(sum + cnt / 2) / cnt] : lv;
                            float ceff = canopy;
                            if (directional_lighting_enabled)
                                ceff = 1.0f - (1.0f - canopy) * cl;
                            corner_b[i] = cl * shades[i] * ceff;
                        }
                    } else {
                        float shadow = (shades[0] + shades[1] + shades[2] + shades[3]) * 0.25f;
                        float ceff = canopy;
                        if (directional_lighting_enabled)
                            ceff = 1.0f - (1.0f - canopy) * lv;
                        for (int i = 0; i < 4; i++)
                            corner_b[i] = lv * shadow * ceff;
                    }
                    if (is_water)
                        add_face(wverts, winds, &wvc, &wic,
                                 (float)x, (float)y, (float)z,
                                 face, type, corner_b, wch);
                    else if (block_transparent(type))
                        add_face(cverts, cinds, &cvc, &cic,
                                 (float)x, (float)y, (float)z,
                                 face, type, corner_b, NULL);
                    else
                        add_face(verts, inds, &vc, &ic,
                                 (float)x, (float)y, (float)z,
                                 face, type, corner_b, NULL);
                }
            }
        }
    }

    *out_solid = mesh_create(verts, vc * 9 * sizeof(GLfloat), inds, ic * sizeof(GLuint));
    *out_water = mesh_create(wverts, wvc * 9 * sizeof(GLfloat), winds, wic * sizeof(GLuint));
    *out_cutout = mesh_create(cverts, cvc * 9 * sizeof(GLfloat), cinds, cic * sizeof(GLuint));
}

Mesh chunk_mesh_build_block(BlockType type) {
    GLfloat verts[6 * 4 * 9];
    GLuint inds[6 * 6];
    int vc = 0, ic = 0;
    float full[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (int face = 0; face < 6; face++)
        add_face(verts, inds, &vc, &ic, 0.0f, 0.0f, 0.0f, face, type, full, NULL);
    return mesh_create(verts, vc * 9 * sizeof(GLfloat), inds, ic * sizeof(GLuint));
}

void chunk_build_mesh_with_neighbors(Chunk* chunk, Chunk* neighbors[4], int dynamic,
    Mesh* out_solid, Mesh* out_water, Mesh* out_cutout) {
    static uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    static uint8_t tmp[4][CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    if (dynamic) {
        Chunk* null_neighbors[4] = {NULL,NULL,NULL,NULL};
        Chunk** nb = neighbors ? neighbors : null_neighbors;
        for (int i = 0; i < 4; i++) {
            if (nb[i]) chunk_compute_lightmap(nb[i], tmp[i]);
            else memset(tmp[i], 0, sizeof(tmp[i]));
        }
        build_lightmap_from_cache(chunk, neighbors,
            nb[0] ? (uint8_t*)tmp[0] : NULL, nb[1] ? (uint8_t*)tmp[1] : NULL,
            nb[2] ? (uint8_t*)tmp[2] : NULL, nb[3] ? (uint8_t*)tmp[3] : NULL, light);
    }
    build_mesh(chunk, neighbors, light, dynamic, out_solid, out_water, out_cutout);
}

void chunk_build_mesh_with_cached_neighbors(Chunk* chunk, Chunk* neighbors[4],
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3, int dynamic,
    Mesh* out_solid, Mesh* out_water, Mesh* out_cutout) {
    static uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    if (dynamic)
        build_lightmap_from_cache(chunk, neighbors, nb0, nb1, nb2, nb3, light);
    build_mesh(chunk, neighbors, light, dynamic, out_solid, out_water, out_cutout);
}