#ifndef CAMERA_H
#define CAMERA_H

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "../include/cglm/include/cglm/cglm.h"
#include "../World/chunk.h"

typedef struct {
    vec3 position;
    vec3 orientation;
    vec3 up;
    vec3 move_forward;
    int  first_click;
    int  width;
    int  height;
    float sensitivity;
    mat4 view;
    mat4 proj;
    mat4 camera_matrix;

    float vel_x;
    float vel_y;
    float vel_z;
    int   jump_buffered;
} Camera;

void camera_init(Camera* cam, int width, int height, vec3 position);
void camera_update(Camera* cam, float fov, float near_plane, float far_plane);
void camera_export(Camera* cam, GLuint shader, const char* uniform);


void camera_rotate(Camera* cam, int mouse_dx, int mouse_dy);


void camera_get_wish_dir(Camera* cam, const Uint8* keys,
                         vec3 wish_dir_out, int* sprinting);

#endif
