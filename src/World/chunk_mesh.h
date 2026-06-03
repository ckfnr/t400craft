#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "chunk.h"
#include "../Mesh/mesh.h"

Mesh chunk_build_mesh(Chunk* chunk);
Mesh chunk_build_mesh_dynamic(Chunk* chunk, int dynamic);

#endif
