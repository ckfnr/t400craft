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

static GLuint compile_shader_source(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static GLuint create_program_from_sources(const char* vertex_source, const char* fragment_source) {
    GLuint vertex_shader = compile_shader_source(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = compile_shader_source(GL_FRAGMENT_SHADER, fragment_source);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

static void append_rect(float* vertices, int* vertex_count, float x, float y, float rect_width, float rect_height) {
    float x2 = x + rect_width;
    float y2 = y + rect_height;

    vertices[(*vertex_count)++] = x;
    vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2;
    vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2;
    vertices[(*vertex_count)++] = y2;

    vertices[(*vertex_count)++] = x;
    vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2;
    vertices[(*vertex_count)++] = y2;
    vertices[(*vertex_count)++] = x;
    vertices[(*vertex_count)++] = y2;
}

static const unsigned char* glyph_rows(char c) {
    static const unsigned char blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const unsigned char c_glyph[7] = {14, 16, 16, 16, 16, 16, 14};
    static const unsigned char e_glyph[7] = {14, 16, 16, 30, 16, 16, 14};
    static const unsigned char g_glyph[7] = {14, 17, 16, 16, 19, 17, 14};
    static const unsigned char i_glyph[7] = {4, 0, 4, 4, 4, 4, 4};
    static const unsigned char l_glyph[7] = {16, 16, 16, 16, 16, 16, 14};
    static const unsigned char n_glyph[7] = {17, 25, 21, 19, 17, 17, 17};
    static const unsigned char o_glyph[7] = {14, 17, 17, 17, 17, 17, 14};
    static const unsigned char p_glyph[7] = {30, 17, 17, 30, 16, 16, 16};
    static const unsigned char t_glyph[7] = {31, 4, 4, 4, 4, 4, 4};
    static const unsigned char u_glyph[7] = {17, 17, 17, 17, 17, 17, 14};
    static const unsigned char y_glyph[7] = {17, 17, 17, 14, 4, 4, 4};
    static const unsigned char a_glyph[7] = {14, 17, 17, 31, 17, 17, 17};

    switch (c) {
        case 'a': return a_glyph;
        case 'c': return c_glyph;
        case 'e': return e_glyph;
        case 'g': return g_glyph;
        case 'i': return i_glyph;
        case 'l': return l_glyph;
        case 'n': return n_glyph;
        case 'o': return o_glyph;
        case 'p': return p_glyph;
        case 't': return t_glyph;
        case 'u': return u_glyph;
        case 'y': return y_glyph;
        default: return blank;
    }
}

static void build_text_vertices(const char* text, float x, float y, float scale, float* vertices, int* vertex_count) {
    float cursor_x = x;
    float pixel = scale;

    for (const char* ch = text; *ch != '\0'; ++ch) {
        if (*ch == ' ') {
            cursor_x += 4.0f * pixel;
            continue;
        }

        const unsigned char* rows = glyph_rows(*ch);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col))) {
                    append_rect(vertices, vertex_count, cursor_x + col * pixel, y + row * pixel, pixel, pixel);
                }
            }
        }

        cursor_x += 6.0f * pixel;
    }
}

static int point_in_rect(int px, int py, float x, float y, float rect_width, float rect_height) {
    return px >= (int)x && px <= (int)(x + rect_width) && py >= (int)y && py <= (int)(y + rect_height);
}

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
    SDL_ShowCursor(SDL_DISABLE);
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

    const char* ui_vert =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "uniform vec2 screenSize;\n"
        "void main()\n"
        "{\n"
        "   vec2 ndc = vec2((aPos.x / screenSize.x) * 2.0 - 1.0, 1.0 - (aPos.y / screenSize.y) * 2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";

    const char* ui_frag =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec4 uColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = uColor;\n"
        "}\n";

    GLuint uiProgram = create_program_from_sources(ui_vert, ui_frag);
    GLuint uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8192, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const char* button_vert =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 texCoord;\n"
        "uniform vec2 screenSize;\n"
        "void main()\n"
        "{\n"
        "   vec2 ndc = vec2((aPos.x / screenSize.x) * 2.0 - 1.0, 1.0 - (aPos.y / screenSize.y) * 2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "   texCoord = aTexCoord;\n"
        "}\n";

    const char* button_frag =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D buttonTex;\n"
        "void main()\n"
        "{\n"
        "   FragColor = texture(buttonTex, texCoord);\n"
        "}\n";

    GLuint buttonProgram = create_program_from_sources(button_vert, button_frag);
    GLuint buttonVAO, buttonVBO;
    glGenVertexArrays(1, &buttonVAO);
    glGenBuffers(1, &buttonVBO);
    glBindVertexArray(buttonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

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

    int buttonWidthImg, buttonHeightImg, buttonNumColCh;
    unsigned char* buttonBytes = stbi_load("src/UI/button.png", &buttonWidthImg, &buttonHeightImg, &buttonNumColCh, 4);

    GLuint buttonTexture;
    glGenTextures(1, &buttonTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, buttonTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buttonWidthImg, buttonHeightImg, 0, GL_RGBA, GL_UNSIGNED_BYTE, buttonBytes);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(buttonBytes);
    glBindTexture(GL_TEXTURE_2D, 0);

    int crosshairWidthImg, crosshairHeightImg, crosshairNumColCh;
    unsigned char* crosshairBytes = stbi_load("src/UI/crosshair.png", &crosshairWidthImg, &crosshairHeightImg, &crosshairNumColCh, 4);

    GLuint crosshairTexture;
    glGenTextures(1, &crosshairTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, crosshairTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, crosshairWidthImg, crosshairHeightImg, 0, GL_RGBA, GL_UNSIGNED_BYTE, crosshairBytes);
    stbi_image_free(crosshairBytes);
    glBindTexture(GL_TEXTURE_2D, 0);

    Camera cam;
    vec3 start_pos = {0.0f, 5.0f, 10.0f};
    camera_init(&cam, width, height, start_pos);

    int running = 1;
    int paused = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                paused = !paused;
                if (paused) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    SDL_ShowCursor(SDL_ENABLE);
                } else {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_ShowCursor(SDL_DISABLE);
                    cam.first_click = 1;
                }
            }
            if (paused && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int button_x = 0;
                int button_y = 0;
                int screen_w = 0;
                int screen_h = 0;
                SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);
                float button_width = 280.0f;
                float button_height = 64.0f;
                button_x = (screen_w - (int)button_width) / 2;
                button_y = (screen_h - (int)button_height) / 2;

                if (point_in_rect(event.button.x, event.button.y, (float)button_x, (float)button_y, button_width, button_height)) {
                    paused = 0;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_ShowCursor(SDL_DISABLE);
                    cam.first_click = 1;
                }
            }
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                glViewport(0, 0, w, h);
                cam.width = w;
                cam.height = h;
            }
        }

        int screen_w = 0;
        int screen_h = 0;
        SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);

        glClearColor(0.4f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        mat4 model;
        glm_mat4_identity(model);

        if (!paused) {
            int mouse_dx = 0, mouse_dy = 0;
            SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            camera_inputs(&cam, keys, mouse_dx, mouse_dy, 0);

            camera_update(&cam, 45.0f, 0.1f, 100.0f);
            camera_export(&cam, shaderProgram, "camMatrix");

            int modelLoc = glGetUniformLocation(shaderProgram, "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
            mesh_draw(&chunk_mesh);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(uiProgram);
            glBindVertexArray(uiVAO);
            glBindBuffer(GL_ARRAY_BUFFER, uiVBO);

            float center_x = (float)screen_w * 0.5f;
            float center_y = (float)screen_h * 0.5f;

            float crosshair_width = 32.0f;
            float crosshair_height = 32.0f;
            float crosshair_x = center_x - crosshair_width * 0.5f;
            float crosshair_y = center_y - crosshair_height * 0.5f;
            float crosshair_vertices[24] = {
                crosshair_x, crosshair_y, 0.0f, 0.0f,
                crosshair_x + crosshair_width, crosshair_y, 1.0f, 0.0f,
                crosshair_x + crosshair_width, crosshair_y + crosshair_height, 1.0f, 1.0f,

                crosshair_x, crosshair_y, 0.0f, 0.0f,
                crosshair_x + crosshair_width, crosshair_y + crosshair_height, 1.0f, 1.0f,
                crosshair_x, crosshair_y + crosshair_height, 0.0f, 1.0f,
            };
            glUseProgram(buttonProgram);
            glBindVertexArray(buttonVAO);
            glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(crosshair_vertices), crosshair_vertices);
            glUniform2f(glGetUniformLocation(buttonProgram, "screenSize"), (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, crosshairTexture);
            glUniform1i(glGetUniformLocation(buttonProgram, "buttonTex"), 2);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
        } else {
            camera_update(&cam, 45.0f, 0.1f, 100.0f);
            camera_export(&cam, shaderProgram, "camMatrix");

            int modelLoc = glGetUniformLocation(shaderProgram, "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
            mesh_draw(&chunk_mesh);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(uiProgram);
            glBindVertexArray(uiVAO);
            glBindBuffer(GL_ARRAY_BUFFER, uiVBO);

            float overlay_vertices[12];
            int overlay_count = 0;
            append_rect(overlay_vertices, &overlay_count, 0.0f, 0.0f, (float)screen_w, (float)screen_h);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * overlay_count, overlay_vertices);
            glUniform2f(glGetUniformLocation(uiProgram, "screenSize"), (float)screen_w, (float)screen_h);
            glUniform4f(glGetUniformLocation(uiProgram, "uColor"), 0.35f, 0.35f, 0.35f, 0.65f);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            float button_width = 280.0f;
            float button_height = 64.0f;
            float button_x = ((float)screen_w - button_width) * 0.5f;
            float button_y = ((float)screen_h - button_height) * 0.5f;

            float button_vertices[24] = {
                button_x, button_y, 0.0f, 0.0f,
                button_x + button_width, button_y, 1.0f, 0.0f,
                button_x + button_width, button_y + button_height, 1.0f, 1.0f,

                button_x, button_y, 0.0f, 0.0f,
                button_x + button_width, button_y + button_height, 1.0f, 1.0f,
                button_x, button_y + button_height, 0.0f, 1.0f,
            };
            glUseProgram(buttonProgram);
            glBindVertexArray(buttonVAO);
            glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(button_vertices), button_vertices);
            glUniform2f(glGetUniformLocation(buttonProgram, "screenSize"), (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, buttonTexture);
            glUniform1i(glGetUniformLocation(buttonProgram, "buttonTex"), 1);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUseProgram(uiProgram);
            glBindVertexArray(uiVAO);
            glBindBuffer(GL_ARRAY_BUFFER, uiVBO);

            float text_vertices[8192];
            int text_count = 0;
            const char* label = "continue playing";
            float text_scale = 2.2f;
            float text_width = 0.0f;
            for (const char* ch = label; *ch != '\0'; ++ch) {
                if (*ch == ' ') {
                    text_width += 4.0f * text_scale;
                } else {
                    text_width += 6.0f * text_scale;
                }
            }
            float text_x = button_x + (button_width - text_width) * 0.5f;
            float text_y = button_y + (button_height - 7.0f * text_scale) * 0.5f;
            build_text_vertices(label, text_x, text_y, text_scale, text_vertices, &text_count);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * text_count, text_vertices);
            glUniform4f(glGetUniformLocation(uiProgram, "uColor"), 0.15f, 0.15f, 0.15f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, text_count / 2);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
        }
        SDL_GL_SwapWindow(window);
    }

    mesh_delete(&chunk_mesh);
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &buttonTexture);
    glDeleteTextures(1, &crosshairTexture);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(uiProgram);
    glDeleteProgram(buttonProgram);
    glDeleteBuffers(1, &uiVBO);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &buttonVBO);
    glDeleteVertexArrays(1, &buttonVAO);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}