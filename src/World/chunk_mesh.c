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
        case BLOCK_AIR:
        default:                return 0;
    }
}

typedef struct { short x, y, z; } LightNode;

static LightNode bfs_queue[CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6];
static uint8_t neighbor_lightmap[4][CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

void chunk_compute_lightmap(Chunk* chunk,
                             uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z]) {
    memset(light, 0, CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z);
    static LightNode local_queue[CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 2];
    int head = 0, tail = 0;
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (chunk->blocks[x][y][z].type != BLOCK_AIR) break;
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
            if (chunk->blocks[nx][ny][nz].type != BLOCK_AIR) continue;
            if (light[nx][ny][nz] >= lv - 1) continue;
            light[nx][ny][nz] = (uint8_t)(lv - 1);
            local_queue[tail++] = (LightNode){(short)nx, (short)ny, (short)nz};
        }
    }
}

static void build_lightmap_from_cache(Chunk* chunk,
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3,
    uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z]) {

    memset(light, 0, CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z);
    int head = 0, tail = 0;

    uint8_t* nbs[4] = {nb0, nb1, nb2, nb3};
    for (int i = 0; i < 4; i++) {
        if (nbs[i])
            memcpy(neighbor_lightmap[i], nbs[i], CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z);
        else
            memset(neighbor_lightmap[i], 0, sizeof(neighbor_lightmap[i]));
    }

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                if (chunk->blocks[x][y][z].type != BLOCK_AIR) break;
                light[x][y][z] = MAX_LIGHT;
                bfs_queue[tail++] = (LightNode){(short)x, (short)y, (short)z};
            }
        }
    }

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            uint8_t nlv = neighbor_lightmap[0][CHUNK_SIZE_X-1][y][z];
            if (nlv > 1 && chunk->blocks[0][y][z].type == BLOCK_AIR) {
                uint8_t inc = (uint8_t)(nlv - 1);
                if (light[0][y][z] < inc) { light[0][y][z] = inc; bfs_queue[tail++] = (LightNode){0,(short)y,(short)z}; }
            }
            nlv = neighbor_lightmap[1][0][y][z];
            if (nlv > 1 && chunk->blocks[CHUNK_SIZE_X-1][y][z].type == BLOCK_AIR) {
                uint8_t inc = (uint8_t)(nlv - 1);
                if (light[CHUNK_SIZE_X-1][y][z] < inc) { light[CHUNK_SIZE_X-1][y][z] = inc; bfs_queue[tail++] = (LightNode){(short)(CHUNK_SIZE_X-1),(short)y,(short)z}; }
            }
        }
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            uint8_t nlv = neighbor_lightmap[2][x][y][CHUNK_SIZE_Z-1];
            if (nlv > 1 && chunk->blocks[x][y][0].type == BLOCK_AIR) {
                uint8_t inc = (uint8_t)(nlv - 1);
                if (light[x][y][0] < inc) { light[x][y][0] = inc; bfs_queue[tail++] = (LightNode){(short)x,(short)y,0}; }
            }
            nlv = neighbor_lightmap[3][x][y][0];
            if (nlv > 1 && chunk->blocks[x][y][CHUNK_SIZE_Z-1].type == BLOCK_AIR) {
                uint8_t inc = (uint8_t)(nlv - 1);
                if (light[x][y][CHUNK_SIZE_Z-1] < inc) { light[x][y][CHUNK_SIZE_Z-1] = inc; bfs_queue[tail++] = (LightNode){(short)x,(short)y,(short)(CHUNK_SIZE_Z-1)}; }
            }
        }
    }

    static const int dx[] = {1,-1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1,-1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1,-1};
    while (head < tail) {
        LightNode n = bfs_queue[head++];
        int lv = light[n.x][n.y][n.z];
        if (lv <= 1) continue;
        for (int d = 0; d < 6; d++) {
            int nx = n.x + dx[d], ny = n.y + dy[d], nz = n.z + dz[d];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
            if (nx < 0 || nx >= CHUNK_SIZE_X || nz < 0 || nz >= CHUNK_SIZE_Z) continue;
            if (chunk->blocks[nx][ny][nz].type != BLOCK_AIR) continue;
            if (light[nx][ny][nz] >= lv - 1) continue;
            light[nx][ny][nz] = (uint8_t)(lv - 1);
            bfs_queue[tail++] = (LightNode){(short)nx, (short)ny, (short)nz};
        }
    }
}

static void add_face(GLfloat* verts, GLuint* inds, int* vc, int* ic,
                     float x, float y, float z, int face, BlockType type, float light) {
    static const float face_dim[] = {1.0f, 0.6f, 0.8f, 0.8f, 0.7f, 0.7f};
    float b = light * face_dim[face];
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

static Mesh build_mesh(Chunk* chunk, Chunk* neighbors[4],
                        uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z],
                        int dynamic) {
    static const int dx[] = {0, 0, 0, 0, -1, 1};
    static const int dy[] = {1,-1, 0, 0,  0, 0};
    static const int dz[] = {0, 0, 1,-1,  0, 0};
    static const float light_table[16] = {
        0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f,
        0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.94f, 0.97f, 1.00f
    };

    GLfloat* verts = malloc(MAX_VERTICES * 9 * sizeof(GLfloat));
    GLuint*  inds  = malloc(MAX_INDICES  * sizeof(GLuint));
    int vc = 0, ic = 0;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                if (chunk->blocks[x][y][z].type == BLOCK_AIR) continue;
                BlockType type = chunk->blocks[x][y][z].type;
                for (int face = 0; face < 6; face++) {
                    int nx = x + dx[face];
                    int ny = y + dy[face];
                    int nz = z + dz[face];
                    int out_of_bounds = (nx < 0 || nx >= CHUNK_SIZE_X ||
                                        ny < 0 || ny >= CHUNK_SIZE_Y ||
                                        nz < 0 || nz >= CHUNK_SIZE_Z);
                    if (!out_of_bounds) {
                        if (chunk->blocks[nx][ny][nz].type != BLOCK_AIR) continue;
                    } else if (!(ny < 0 || ny >= CHUNK_SIZE_Y)) {
                        Chunk* nbr = NULL;
                        int lx = nx, lnz = nz;
                        if      (nx < 0)             { nbr = neighbors ? neighbors[0] : NULL; lx  = CHUNK_SIZE_X-1; }
                        else if (nx >= CHUNK_SIZE_X) { nbr = neighbors ? neighbors[1] : NULL; lx  = 0; }
                        else if (nz < 0)             { nbr = neighbors ? neighbors[2] : NULL; lnz = CHUNK_SIZE_Z-1; }
                        else                         { nbr = neighbors ? neighbors[3] : NULL; lnz = 0; }
                        if (nbr && chunk_get_block(nbr, lx, ny, lnz) &&
                            chunk_get_block(nbr, lx, ny, lnz)->type != BLOCK_AIR) continue;
                    }
                    float lv;
                    if (!dynamic) {
                        lv = 1.0f;
                    } else if (!out_of_bounds) {
                        lv = light_table[light[nx][ny][nz]];
                    } else if (ny < 0 || ny >= CHUNK_SIZE_Y) {
                        lv = 1.0f;
                    } else if (nx < 0) {
                        lv = light_table[neighbor_lightmap[0][CHUNK_SIZE_X-1][ny][nz]];
                    } else if (nx >= CHUNK_SIZE_X) {
                        lv = light_table[neighbor_lightmap[1][0][ny][nz]];
                    } else if (nz < 0) {
                        lv = light_table[neighbor_lightmap[2][nx][ny][CHUNK_SIZE_Z-1]];
                    } else {
                        lv = light_table[neighbor_lightmap[3][nx][ny][0]];
                    }
                    add_face(verts, inds, &vc, &ic,
                             (float)x, (float)y, (float)z,
                             face, type, lv);
                }
            }
        }
    }

    Mesh m = mesh_create(verts, vc * 9 * sizeof(GLfloat), inds, ic * sizeof(GLuint));
    free(verts); free(inds);
    return m;
}

Mesh chunk_build_mesh_with_neighbors(Chunk* chunk, Chunk* neighbors[4], int dynamic) {
    static uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    static uint8_t tmp[4][CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    if (dynamic) {
        Chunk* null_neighbors[4] = {NULL,NULL,NULL,NULL};
        Chunk** nb = neighbors ? neighbors : null_neighbors;
        for (int i = 0; i < 4; i++) {
            if (nb[i]) chunk_compute_lightmap(nb[i], tmp[i]);
            else memset(tmp[i], 0, sizeof(tmp[i]));
        }
        build_lightmap_from_cache(chunk,
            (uint8_t*)tmp[0], (uint8_t*)tmp[1],
            (uint8_t*)tmp[2], (uint8_t*)tmp[3], light);
    }
    return build_mesh(chunk, neighbors, light, dynamic);
}

Mesh chunk_build_mesh_with_cached_neighbors(Chunk* chunk, Chunk* neighbors[4],
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3, int dynamic) {
    static uint8_t light[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    if (dynamic)
        build_lightmap_from_cache(chunk, nb0, nb1, nb2, nb3, light);
    return build_mesh(chunk, neighbors, light, dynamic);
}

Mesh chunk_build_mesh_dynamic(Chunk* chunk, int dynamic) {
    return chunk_build_mesh_with_neighbors(chunk, NULL, dynamic);
}

Mesh chunk_build_mesh(Chunk* chunk) {
    return chunk_build_mesh_dynamic(chunk, 1);
}