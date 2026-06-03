#include "chunk_mesh.h"
#include <stdlib.h>

#define MAX_VERTICES (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 4)
#define MAX_INDICES  (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 6 * 6)

static int block_face_texture_layer(BlockType type, int face) {
    switch (type) {
        case BLOCK_DIRT:        return 0;
        case BLOCK_GRASS:
            if (face == 0) return 2;
            if (face == 1) return 0;
            return 1;
        case BLOCK_COBBLESTONE: return 3;
        case BLOCK_STONE:       return 3;
        case BLOCK_AIR:
        default:                return 0;
    }
}

static int block_has_sky(Chunk* chunk, int x, int y, int z) {
    Block* above = chunk_get_block(chunk, x, y + 1, z);
    return (above == NULL || above->type == BLOCK_AIR);
}

static void add_face(GLfloat* verts, GLuint* inds, int* vc, int* ic,
                     float x, float y, float z, int face, BlockType type,
                     int sky, int dynamic) {
    float face_dim[] = {1.0f, 0.6f, 0.8f, 0.8f, 0.7f, 0.7f};
    float b = face_dim[face];
    if (dynamic && !sky) b *= 0.3f;
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
            positions[0][0]=x;   positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x+1; positions[1][1]=y;   positions[1][2]=z;
            positions[2][0]=x+1; positions[2][1]=y;   positions[2][2]=z+1;
            positions[3][0]=x;   positions[3][1]=y;   positions[3][2]=z+1;
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
            positions[0][0]=x;   positions[0][1]=y;   positions[0][2]=z;
            positions[1][0]=x;   positions[1][1]=y;   positions[1][2]=z+1;
            positions[2][0]=x;   positions[2][1]=y+1; positions[2][2]=z+1;
            positions[3][0]=x;   positions[3][1]=y+1; positions[3][2]=z;
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

Mesh chunk_build_mesh_dynamic(Chunk* chunk, int dynamic) {
    GLfloat* verts = malloc(MAX_VERTICES * 9 * sizeof(GLfloat));
    GLuint*  inds  = malloc(MAX_INDICES  * sizeof(GLuint));
    int vc = 0, ic = 0;

    int dx[] = {0, 0, 0, 0, -1, 1};
    int dy[] = {1,-1, 0, 0,  0, 0};
    int dz[] = {0, 0, 1,-1,  0, 0};

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                if (chunk->blocks[x][y][z].type == BLOCK_AIR) continue;
                BlockType type = chunk->blocks[x][y][z].type;
                int sky = block_has_sky(chunk, x, y, z);
                for (int face = 0; face < 6; face++) {
                    int nx = x + dx[face];
                    int ny = y + dy[face];
                    int nz = z + dz[face];
                    Block* neighbor = chunk_get_block(chunk, nx, ny, nz);
                    if (neighbor != NULL && neighbor->type != BLOCK_AIR) continue;
                    int face_sky = sky;
                    if (face >= 2) {
                        face_sky = block_has_sky(chunk, nx, y, nz);
                        if (face_sky == 0) face_sky = sky;
                    }
                    add_face(verts, inds, &vc, &ic, (float)x, (float)y, (float)z, face, type, face_sky, dynamic);
                }
            }
        }
    }

    Mesh m = mesh_create(verts, vc * 9 * sizeof(GLfloat), inds, ic * sizeof(GLuint));
    free(verts);
    free(inds);
    return m;
}

Mesh chunk_build_mesh(Chunk* chunk) {
    return chunk_build_mesh_dynamic(chunk, 1);
}
