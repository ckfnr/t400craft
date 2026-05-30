#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <math.h>
#include "Shaders/shader_loader.h"
#include "Mesh/mesh.h"
#include "include/stb_image.h"

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
        800, 800,
        SDL_WINDOW_OPENGL
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    glewInit();

    GLfloat vertices[] = {
        // position             // color            // texture
        -0.5f,  0.5f, 0.0f,   1.0f, 0.6f, 0.32f,  0.0f, 1.0f,  // upper left
        0.5f,  0.5f, 0.0f,   1.0f, 0.6f, 0.32f,  1.0f, 1.0f,  // upper right
        0.5f, -0.5f, 0.0f,   0.8f, 0.3f, 0.02f,  1.0f, 0.0f,  // lower right
        -0.5f, -0.5f, 0.0f,   0.8f, 0.3f, 0.02f,  0.0f, 0.0f,  // lower left
    };

    GLuint indices[] = {
        0, 1, 2,
        0, 2, 3
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
    unsigned char* bytes = stbi_load("src/textures/ugly_bricks.png", &widthImg, &heightImg, &numColCh, 0);      //image path

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
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform1f(uniID, 0.0f);  //last variable = scale
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
        mesh_draw(&mesh);
        SDL_GL_SwapWindow(window);
    }

    mesh_delete(&mesh);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}