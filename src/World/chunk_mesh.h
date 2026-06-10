#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "chunk.h"
#include "../Mesh/mesh.h"
#include <stdint.h>

void chunk_build_mesh_with_neighbors(Chunk* chunk, Chunk* neighbors[4], int dynamic,
    Mesh* out_solid, Mesh* out_water);
void chunk_build_mesh_with_cached_neighbors(Chunk* chunk, Chunk* neighbors[4],
    uint8_t* nb0, uint8_t* nb1, uint8_t* nb2, uint8_t* nb3, int dynamic,
    Mesh* out_solid, Mesh* out_water);
void chunk_compute_lightmap(Chunk* chunk, uint8_t out[16][128][16]);

#endif