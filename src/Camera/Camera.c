#include "Camera.h"
#include <SDL2/SDL.h>
#include <math.h>

void camera_init(Camera* cam, int width, int height, vec3 position) {
    cam->width = width;
    cam->height = height;
    glm_vec3_copy(position, cam->position);
    cam->orientation[0] = 0.0f; cam->orientation[1] = 0.0f; cam->orientation[2] = -1.0f;
    cam->up[0] = 0.0f; cam->up[1] = 1.0f; cam->up[2] = 0.0f;
    glm_vec3_copy(cam->orientation, cam->move_forward);
    glm_mat4_identity(cam->view);
    glm_mat4_identity(cam->proj);
    glm_mat4_identity(cam->camera_matrix);
    cam->first_click = 1;
    cam->speed = 0.1f;
    cam->sensitivity = 100.0f;
}

void camera_update(Camera* cam, float fov, float near_plane, float far_plane) {
    vec3 target;
    glm_vec3_add(cam->position, cam->orientation, target);
    glm_lookat(cam->position, target, cam->up, cam->view);
    glm_perspective(glm_rad(fov), (float)cam->width / cam->height, near_plane, far_plane, cam->proj);
    glm_mat4_mul(cam->proj, cam->view, cam->camera_matrix);
}

void camera_export(Camera* cam, GLuint shader, const char* uniform) {
    glUniformMatrix4fv(glGetUniformLocation(shader, uniform), 1, GL_FALSE, (float*)cam->camera_matrix);
}

void camera_inputs(Camera* cam, const Uint8* keys, int mouse_dx, int mouse_dy, int mouse_held) {
    vec3 tmp;
    vec3 world_up = {0.0f, 1.0f, 0.0f};

    if (keys[SDL_SCANCODE_W]) {
        glm_vec3_scale(cam->move_forward, cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }
    if (keys[SDL_SCANCODE_S]) {
        glm_vec3_scale(cam->move_forward, -cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }
    if (keys[SDL_SCANCODE_A]) {
        glm_vec3_cross(cam->move_forward, world_up, tmp);
        glm_vec3_normalize(tmp);
        glm_vec3_scale(tmp, -cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }
    if (keys[SDL_SCANCODE_D]) {
        glm_vec3_cross(cam->move_forward, world_up, tmp);
        glm_vec3_normalize(tmp);
        glm_vec3_scale(tmp, cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }
    if (keys[SDL_SCANCODE_SPACE]) {
        glm_vec3_scale(world_up, cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }
    if (keys[SDL_SCANCODE_LSHIFT]) {
        glm_vec3_scale(world_up, -cam->speed, tmp);
        glm_vec3_add(cam->position, tmp, cam->position);
    }

    cam->speed = keys[SDL_SCANCODE_LCTRL] ? 0.4f : 0.1f;

    if (mouse_held) {
        if (cam->first_click) {
            cam->first_click = 0;
            return;
        }
        float rotX = cam->sensitivity * (float)mouse_dy / cam->height;
        float rotY = cam->sensitivity * (float)mouse_dx / cam->width;

        vec3 right, new_orientation;
        glm_vec3_cross(cam->orientation, cam->up, right);
        glm_vec3_normalize(right);
        glm_vec3_copy(cam->orientation, new_orientation);
        glm_vec3_rotate(new_orientation, glm_rad(-rotX), right);

        float angle = glm_vec3_angle(new_orientation, cam->up);
        if (fabsf(angle - (float)GLM_PI_2) <= glm_rad(85.0f))
            glm_vec3_copy(new_orientation, cam->orientation);

        glm_vec3_rotate(cam->orientation, glm_rad(-rotY), cam->up);
    } else {
        cam->first_click = 1;
    }

    vec3 horizontal_forward = {cam->orientation[0], 0.0f, cam->orientation[2]};
    if (glm_vec3_norm(horizontal_forward) > 0.0001f) {
        glm_vec3_normalize(horizontal_forward);
        glm_vec3_copy(horizontal_forward, cam->move_forward);
    }
}