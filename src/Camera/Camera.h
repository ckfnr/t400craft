#ifndef CAMERA_H
#define CAMERA_H

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "../include/cglm/include/cglm/cglm.h"

typedef struct {
    vec3 position;
    vec3 orientation;
    vec3 up;
    int first_click;
    int width;
    int height;
    float speed;
    float sensitivity;
    mat4 view;
    mat4 proj;
    mat4 camera_matrix;
} Camera;

void camera_init(Camera* cam, int width, int height, vec3 position);
void camera_matrix(Camera* cam, float fov, float near, float far, GLuint shader, const char* uniform);
void camera_inputs(Camera* cam, const Uint8* keys, int mouse_dx, int mouse_dy, int mouse_held);
void camera_update(Camera* cam, float fov, float near_plane, float far_plane);
void camera_export(Camera* cam, GLuint shader, const char* uniform);

#endif