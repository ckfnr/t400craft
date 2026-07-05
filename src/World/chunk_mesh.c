#include "chunk_mesh.h"
#include <stdlib.h>
#include <string.h>

#define MAX_VERTICES (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 4)
#define MAX_INDICES  (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 6)
#define MAX_LIGHT 15

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
        case BLOCK_AIR:
        default:                return 0;
    }
}

typedef struct { short x, y, z; } LightNode;

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
                if (block_opaque(chunk->blocks[x][y][z].type)) break;
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
            if (block_opaque(chunk->blocks[nx][ny][nz].type)) continue;
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
                ext_solid[x+1][y][z+1] = (uint8_t)block_opaque(chunk->blocks[x][y][z].type);

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            if (nbc[0]) ext_solid[0][y][z+1]       = (uint8_t)block_opaque(nbc[0]->blocks[CHUNK_SIZE_X-1][y][z].type);
            if (nbc[1]) ext_solid[EXT_X-1][y][z+1] = (uint8_t)block_opaque(nbc[1]->blocks[0][y][z].type);
        }
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            if (nbc[2]) ext_solid[x+1][y][0]       = (uint8_t)block_opaque(nbc[2]->blocks[x][y][CHUNK_SIZE_Z-1].type);
            if (nbc[3]) ext_solid[x+1][y][EXT_Z-1] = (uint8_t)block_opaque(nbc[3]->blocks[x][y][0].type);
        }
    }

    int head = 0, tail = 0;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (block_opaque(chunk->blocks[x][y][z].type)) break;
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

static void add_face(GLfloat* verts, GLuint* inds, int* vc, int* ic,
                     float x, float y, float z, int face, BlockType type, float light, float shadow, const float* ch) {
    static const float face_dim[] = {1.0f, 0.6f, 0.8f, 0.8f, 0.7f, 0.7f};
    float b = light * face_dim[face] * shadow;
    float layer = (float)block_face_texture_layer(type, face);
    float positions[4][3];
    switch (face) {
        case 0:
            positions[0][0]=x;   positions[0][1]=y+1; positions[0][2]=z+1;
            positions[1][0]=x+1; positions[1][1]=y+1; positions[1][2]=z+1;
            positions[2][0]=x+1; positions[2][1]=y+1; positions[2][2]=z;
            positions[3][0]=x;   positions[3][1]=y+1; positions[3][2]=z;
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
            positions[2][0]=x+1; positions[2][1]=y+1; positions[2][2]=z+1;
            positions[3][0]=x;   positions[3][1]=y+1; positions[3][2]=z+1;
            break;
        case 3:
            positions[0][0]=x+1; positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x;   positions[1][1]=y;   positions[1][2]=z;
            positions[2][0]=x;   positions[2][1]=y+1; positions[2][2]=z;
            positions[3][0]=x+1; positions[3][1]=y+1; positions[3][2]=z;
            break;
        case 4:
            positions[0][0]=x; positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x; positions[1][1]=y;   positions[1][2]=z+1;
            positions[2][0]=x; positions[2][1]=y+1; positions[2][2]=z+1;
            positions[3][0]=x; positions[3][1]=y+1; positions[3][2]=z;
            break;
        default:
            positions[0][0]=x+1; positions[0][1]=y;   positions[0][2]=z+1;
            positions[1][0]=x+1; positions[1][1]=y;   positions[1][2]=z;
            positions[2][0]=x+1; positions[2][1]=y+1; positions[2][2]=z;
            positions[3][0]=x+1; positions[3][1]=y+1; positions[3][2]=z+1;
            break;
    }
    if (ch) {
        for (int i = 0; i < 4; i++) {
            if (positions[i][1] > y + 0.5f) {
                int xi = positions[i][0] > x + 0.5f;
                int zi = positions[i][2] > z + 0.5f;
                positions[i][1] = y + ch[xi * 2 + zi];
            }
        }
    }
    float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    if (face >= 2) {
        float flipped[4][2] = {{0,1},{1,1},{1,0},{0,0}};
        for (int i = 0; i < 4; ++i) { uvs[i][0]=flipped[i][0]; uvs[i][1]=flipped[i][1]; }
    }
    int base = *vc;
    for (int i = 0; i < 4; i++) {
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
    if (nx < 0)             return neighbors[0] ? &neighbors[0]->blocks[CHUNK_SIZE_X-1][ny][nz] : NULL;
    if (nx >= CHUNK_SIZE_X) return neighbors[1] ? &neighbors[1]->blocks[0][ny][nz] : NULL;
    if (nz < 0)             return neighbors[2] ? &neighbors[2]->blocks[nx][ny][CHUNK_SIZE_Z-1] : NULL;
    return                         neighbors[3] ? &neighbors[3]->blocks[nx][ny][0] : NULL;
}

static int mesh_cell_opaque(Chunk* chunk, Chunk* neighbors[4], int nx, int ny, int nz) {
    Block* b = mesh_cell(chunk, neighbors, nx, ny, nz);
    return b && block_opaque(b->type);
}

static float face_shadow_factor(Chunk* chunk, Chunk* neighbors[4], int x, int y, int z, int face) {
    if (face == 0) return 1.0f;
    static const int face_axes[6][6] = {
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0},
        {1, 0, 0, 0, 1, 0},
        {0, 1, 0, 0, 0, 1},
        {0, 1, 0, 0, 0, 1},
    };
    static const int corner_signs[4][2] = {
        {-1, -1},
        { 1, -1},
        { 1,  1},
        {-1,  1},
    };

    const int* axis = face_axes[face];
    int ux = axis[0], uy = axis[1], uz = axis[2];
    int vx = axis[3], vy = axis[4], vz = axis[5];
    float total = 0.0f;

    for (int i = 0; i < 4; i++) {
        int su = corner_signs[i][0];
        int sv = corner_signs[i][1];
        int side_u = mesh_cell_opaque(chunk, neighbors, x + su * ux, y + su * uy, z + su * uz);
        int side_v = mesh_cell_opaque(chunk, neighbors, x + sv * vx, y + sv * vy, z + sv * vz);
        int corner = mesh_cell_opaque(chunk, neighbors,
            x + su * ux + sv * vx,
            y + su * uy + sv * vy,
            z + su * uz + sv * vz);

        float shade = 1.0f;
        shade -= 0.16f * (float)(side_u + side_v);
        shade -= 0.10f * (float)corner;
        if (side_u && side_v) shade -= 0.10f;
        if (shade < 0.42f) shade = 0.42f;
        total += shade;
    }

    return total * 0.25f;
}

static float canopy_shadow_factor(Chunk* chunk, Chunk* neighbors[4], int x, int y, int z) {
    float occlusion = 0.0f;
    for (int dy = 1; dy <= 8 && y + dy < CHUNK_SIZE_Y; dy++) {
        float height_weight = 1.0f / (float)dy;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                Block* b = mesh_cell(chunk, neighbors, x + dx, y + dy, z + dz);
                if (!b || !block_opaque(b->type)) continue;
                float footprint = 1.0f;
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
                        int dynamic, Mesh* out_solid, Mesh* out_water) {
    static const int dx[] = {0, 0, 0, 0, -1, 1};
    static const int dy[] = {1,-1, 0, 0,  0, 0};
    static const int dz[] = {0, 0, 1,-1,  0, 0};
    static const float light_table[16] = {
        0.07f, 0.09f, 0.12f, 0.15f, 0.19f, 0.24f, 0.30f, 0.37f,
        0.45f, 0.54f, 0.63f, 0.72f, 0.81f, 0.89f, 0.95f, 1.00f
    };

    static GLfloat verts[MAX_VERTICES * 9];
    static GLuint  inds[MAX_INDICES];
    static GLfloat wverts[MAX_VERTICES * 9];
    static GLuint  winds[MAX_INDICES];
    int vc = 0, ic = 0, wvc = 0, wic = 0;

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
                        if (have_n && ntype != BLOCK_AIR) continue;
                    } else {
                        if (have_n && block_opaque(ntype)) continue;
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
                    float shadow = face_shadow_factor(chunk, neighbors, x, y, z, face);
                    if (face == 0)
                        shadow *= canopy_shadow_factor(chunk, neighbors, x, y, z);
                    if (is_water)
                        add_face(wverts, winds, &wvc, &wic,
                                 (float)x, (float)y, (float)z,
                                 face, type, lv, shadow, wch);
                    else
                        add_face(verts, inds, &vc, &ic,
                                 (float)x, (float)y, (float)z,
                                 face, type, lv, shadow, NULL);
                }
            }
        }
    }

    *out_solid = mesh_create(verts, vc * 9 * sizeof(GLfloat), inds, ic * sizeof(GLuint));
    *out_water = mesh_create(wverts, wvc * 9 * sizeof(GLfloat), winds, wic * sizeof(GLuint));
}

void chunk_build_mesh_with_neighbors(Chunk* chunk, Chunk* neighbors[4], int dynamic,
    Mesh* out_solid, Mesh* out_water) {
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
    build_mesh(chunk, neighbors, light, dynamic, out_solid, out_water);
}

void chunk_build_mesh_with_cached_neighbors(Chunk* chunk, Chunk* neighbors[4],
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3, int dynamic,
    Mesh* out_solid, Mesh* out_water) {
    static uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    if (dynamic)
        build_lightmap_from_cache(chunk, neighbors, nb0, nb1, nb2, nb3, light);
    build_mesh(chunk, neighbors, light, dynamic, out_solid, out_water);
}