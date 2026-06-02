#include "Camera.h"
#include <SDL2/SDL.h>
#include <math.h>

void camera_init(Camera* cam, int width, int height, vec3 position) {
    cam->width      = width;
    cam->height     = height;
    glm_vec3_copy(position, cam->position);
    cam->orientation[0] = 0.0f; cam->orientation[1] = 0.0f; cam->orientation[2] = -1.0f;
    cam->up[0]      = 0.0f; cam->up[1] = 1.0f; cam->up[2] = 0.0f;
    glm_vec3_copy(cam->orientation, cam->move_forward);
    glm_mat4_identity(cam->view);
    glm_mat4_identity(cam->proj);
    glm_mat4_identity(cam->camera_matrix);
    cam->first_click   = 1;
    cam->sensitivity   = 100.0f;
    cam->vel_x         = 0.0f;
    cam->vel_y         = 0.0f;
    cam->vel_z         = 0.0f;
    cam->jump_buffered = 0;
}

void camera_update(Camera* cam, float fov, float near_plane, float far_plane) {
    vec3 target;
    glm_vec3_add(cam->position, cam->orientation, target);
    glm_lookat(cam->position, target, cam->up, cam->view);
    glm_perspective(glm_rad(fov),
                    (float)cam->width / (float)cam->height,
                    near_plane, far_plane, cam->proj);
    glm_mat4_mul(cam->proj, cam->view, cam->camera_matrix);
}

void camera_export(Camera* cam, GLuint shader, const char* uniform) {
    glUniformMatrix4fv(glGetUniformLocation(shader, uniform),
                       1, GL_FALSE, (float*)cam->camera_matrix);
}

void camera_rotate(Camera* cam, int mouse_dx, int mouse_dy) {
    if (cam->first_click) {
        cam->first_click = 0;
        return;
    }

    float rotX = cam->sensitivity * (float)mouse_dy / (float)cam->height;
    float rotY = cam->sensitivity * (float)mouse_dx / (float)cam->width;

    vec3 right;
    glm_vec3_cross(cam->orientation, cam->up, right);
    glm_vec3_normalize(right);
    vec3 new_orientation;
    glm_vec3_copy(cam->orientation, new_orientation);
    glm_vec3_rotate(new_orientation, glm_rad(-rotX), right);

    float angle = glm_vec3_angle(new_orientation, cam->up);
    if (fabsf(angle - (float)GLM_PI_2) <= glm_rad(85.0f))
        glm_vec3_copy(new_orientation, cam->orientation);

    glm_vec3_rotate(cam->orientation, glm_rad(-rotY), cam->up);

    vec3 hfwd = {cam->orientation[0], 0.0f, cam->orientation[2]};
    if (glm_vec3_norm(hfwd) > 0.0001f) {
        glm_vec3_normalize(hfwd);
        glm_vec3_copy(hfwd, cam->move_forward);
    }
}

void camera_get_wish_dir(Camera* cam, const Uint8* keys,
                         vec3 wish_dir_out, int* sprinting) {
    vec3 world_up = {0.0f, 1.0f, 0.0f};
    vec3 right;
    glm_vec3_cross(cam->move_forward, world_up, right);
    glm_vec3_normalize(right);

    glm_vec3_zero(wish_dir_out);
    vec3 tmp;

    if (keys[SDL_SCANCODE_W]) { glm_vec3_add(wish_dir_out, cam->move_forward, wish_dir_out); }
    if (keys[SDL_SCANCODE_S]) { glm_vec3_scale(cam->move_forward, -1.0f, tmp); glm_vec3_add(wish_dir_out, tmp, wish_dir_out); }
    if (keys[SDL_SCANCODE_D]) { glm_vec3_add(wish_dir_out, right, wish_dir_out); }
    if (keys[SDL_SCANCODE_A]) { glm_vec3_scale(right, -1.0f, tmp); glm_vec3_add(wish_dir_out, tmp, wish_dir_out); }

    wish_dir_out[1] = 0.0f;
    if (glm_vec3_norm(wish_dir_out) > 0.0001f)
        glm_vec3_normalize(wish_dir_out);

    *sprinting = keys[SDL_SCANCODE_LCTRL] ? 1 : 0;
}
