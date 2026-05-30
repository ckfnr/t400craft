#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>

typedef struct {
    GLuint VAO, VBO, EBO;
} Mesh;

Mesh mesh_create(GLfloat* vertices, GLsizeiptr vert_size,
                 GLuint* indices,   GLsizeiptr ind_size);
void mesh_draw(Mesh* mesh);
void mesh_delete(Mesh* mesh);

#endif