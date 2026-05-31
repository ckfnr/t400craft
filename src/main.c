#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <math.h>
#include "Shaders/shader_loader.h"
#include "Mesh/mesh.h"
#include "include/stb_image.h"
#include "include/cglm/include/cglm/cglm.h"

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
        SDL_WINDOW_OPENGL
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    glewInit();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

GLfloat vertices[] = {
    // front
    -0.5f, -0.5f,  0.5f,   1.0f, 0.6f, 0.32f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   1.0f, 0.6f, 0.32f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   1.0f, 0.6f, 0.32f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,   1.0f, 0.6f, 0.32f,  0.0f, 1.0f,
    // back
    -0.5f, -0.5f, -0.5f,   0.8f, 0.3f, 0.02f,  1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   0.8f, 0.3f, 0.02f,  0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   0.8f, 0.3f, 0.02f,  0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.8f, 0.3f, 0.02f,  1.0f, 1.0f,
    // left
    -0.5f, -0.5f, -0.5f,   0.9f, 0.45f, 0.17f,  0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,   0.9f, 0.45f, 0.17f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,   0.9f, 0.45f, 0.17f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.9f, 0.45f, 0.17f,  0.0f, 1.0f,
    // right
     0.5f, -0.5f,  0.5f,   0.9f, 0.45f, 0.17f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   0.9f, 0.45f, 0.17f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   0.9f, 0.45f, 0.17f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,   0.9f, 0.45f, 0.17f,  0.0f, 1.0f,
    // top
    -0.5f,  0.5f,  0.5f,   0.6f, 0.8f, 0.2f,   0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,   0.6f, 0.8f, 0.2f,   1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,   0.6f, 0.8f, 0.2f,   1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,   0.6f, 0.8f, 0.2f,   0.0f, 1.0f,
    // bottom
    -0.5f, -0.5f, -0.5f,   0.5f, 0.3f, 0.1f,   0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,   0.5f, 0.3f, 0.1f,   1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,   0.5f, 0.3f, 0.1f,   1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,   0.5f, 0.3f, 0.1f,   0.0f, 1.0f,
};

GLuint indices[] = {
     0,  1,  2,   0,  2,  3,  // front
     4,  6,  5,   4,  7,  6,  // back
     8,  9, 10,   8, 10, 11,  // left
    12, 13, 14,  12, 14, 15,  // right
    16, 17, 18,  16, 18, 19,  // top
    20, 21, 22,  20, 22, 23,  // bottom
};

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

    Mesh mesh = mesh_create(vertices, sizeof(vertices), indices, sizeof(indices));

    int running = 1;
    SDL_Event event;

    GLuint uniID = glGetUniformLocation(shaderProgram, "scale");

    //Texture
    int widthImg, heightImg, numColCh;
    unsigned char* bytes = stbi_load("src/textures/ugly_bricks.png", &widthImg, &heightImg, &numColCh, 4);      //image path

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


    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        mat4 model;
        mat4 view;
        mat4 proj;

        float angle = (float)SDL_GetTicks() / 1000.0f;

        glm_mat4_identity(model);
        glm_rotate(model, angle, (vec3){0.0f, 1.0f, 0.0f});  // live rotation
        // glm_rotate(model, glm_rad(30.0f), (vec3){1.0f, 0.0f, 0.0f});  // static x
        // glm_rotate(model, glm_rad(45.0f), (vec3){0.0f, 1.0f, 0.0f});  // static y

        glm_mat4_identity(view);
        glm_mat4_identity(proj);
        glm_translate(view, (vec3){0.0f, 0.0f, -3.0f});
        glm_perspective(glm_rad(45.0f), (float)(width/height), 0.1f, 100.0f, proj);  // fixed fov

        int modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);
        int viewLoc = glGetUniformLocation(shaderProgram, "view");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (float*)view);
        int projLoc = glGetUniformLocation(shaderProgram, "proj");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, (float*)proj);

        glUniform1f(uniID, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
        mesh_draw(&mesh);
        SDL_GL_SwapWindow(window);
    }

    mesh_delete(&mesh);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}