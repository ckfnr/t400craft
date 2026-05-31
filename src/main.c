#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <math.h>
#include "Shaders/shader_loader.h"
#include "Mesh/mesh.h"
#include "include/stb_image.h"
#include "include/cglm/include/cglm/cglm.h"
#include "Camera/Camera.h"
#include "World/chunk.h"
#include "World/chunk_mesh.h"

const unsigned int width = 800;
const unsigned int height = 800;

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow(
        "t400craft",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    glewInit();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    //glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    char* vertSrc = load_file("src/Shaders/default.vert");
    char* fragSrc = load_file("src/Shaders/default.frag");
    glShaderSource(vertexShader, 1, (const char**)&vertSrc, NULL);
    glCompileShader(vertexShader);
    glShaderSource(fragmentShader, 1, (const char**)&fragSrc, NULL);
    glCompileShader(fragmentShader);
    free(vertSrc);
    free(fragSrc);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    Chunk chunk;
    chunk_init(&chunk, 0, 0);
    Mesh chunk_mesh = chunk_build_mesh(&chunk);

    int widthImg, heightImg, numColCh;
    unsigned char* bytes = stbi_load("src/textures/dirtblock.png", &widthImg, &heightImg, &numColCh, 4); //texture

    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, widthImg, heightImg, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(bytes);
    glBindTexture(GL_TEXTURE_2D, 0);

    Camera cam;
    vec3 start_pos = {0.0f, 5.0f, 10.0f};
    camera_init(&cam, width, height, start_pos);

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                glViewport(0, 0, w, h);
                cam.width = w;
                cam.height = h;
            }
        }

        glClearColor(0.4f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        int mouse_dx = 0, mouse_dy = 0;
        Uint32 mouse_state = SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
        int mouse_held = (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        camera_inputs(&cam, keys, mouse_dx, mouse_dy, mouse_held);

        mat4 model;
        glm_mat4_identity(model);

        camera_update(&cam, 45.0f, 0.1f, 100.0f);
        camera_export(&cam, shaderProgram, "camMatrix");

        int modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
        mesh_draw(&chunk_mesh);
        SDL_GL_SwapWindow(window);
    }

    mesh_delete(&chunk_mesh);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}