#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <float.h>
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

static GLuint load_texture_array(const char* const* paths, int layer_count) {
    int width = 0;
    int height = 0;
    unsigned char* layer_data[layer_count];

    for (int i = 0; i < layer_count; ++i) {
        layer_data[i] = stbi_load(paths[i], &width, &height, NULL, 4);
        if (layer_data[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                stbi_image_free(layer_data[j]);
            }
            return 0;
        }
    }

    GLuint texture_array;
    glGenTextures(1, &texture_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, width, height, layer_count, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    for (int i = 0; i < layer_count; ++i) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, layer_data[i]);
        stbi_image_free(layer_data[i]);
    }

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return texture_array;
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
    static const unsigned char r_glyph[7] = {30, 17, 17, 30, 20, 18, 17};
    static const unsigned char t_glyph[7] = {31, 4, 4, 4, 4, 4, 4};
    static const unsigned char u_glyph[7] = {17, 17, 17, 17, 17, 17, 14};
    static const unsigned char v_glyph[7] = {17, 17, 17, 17, 17, 10, 4};
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
        case 'r': return r_glyph;
        case 't': return t_glyph;
        case 'u': return u_glyph;
        case 'v': return v_glyph;
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

static int block_is_solid(BlockType type) {
    return type != BLOCK_AIR;
}

typedef struct {
    int hit;
    int block_x;
    int block_y;
    int block_z;
    int place_x;
    int place_y;
    int place_z;
} BlockSelection;

static void rebuild_chunk_mesh(Chunk* chunk, Mesh* chunk_mesh) {
    mesh_delete(chunk_mesh);
    *chunk_mesh = chunk_build_mesh(chunk);
}

static int set_block_type(Chunk* chunk, int x, int y, int z, BlockType type) {
    Block* block = chunk_get_block(chunk, x, y, z);
    if (block == NULL) {
        return 0;
    }

    block->type = type;
    return 1;
}

static int raycast_block_selection(Chunk* chunk, vec3 origin, vec3 direction, float max_distance, BlockSelection* selection) {
    vec3 ray_direction;
    glm_vec3_copy(direction, ray_direction);
    if (glm_vec3_norm(ray_direction) < 0.0001f) {
        return 0;
    }
    glm_vec3_normalize(ray_direction);

    int map_x = (int)floorf(origin[0]);
    int map_y = (int)floorf(origin[1]);
    int map_z = (int)floorf(origin[2]);

    int step_x = ray_direction[0] > 0.0f ? 1 : (ray_direction[0] < 0.0f ? -1 : 0);
    int step_y = ray_direction[1] > 0.0f ? 1 : (ray_direction[1] < 0.0f ? -1 : 0);
    int step_z = ray_direction[2] > 0.0f ? 1 : (ray_direction[2] < 0.0f ? -1 : 0);

    float t_max_x = FLT_MAX;
    float t_max_y = FLT_MAX;
    float t_max_z = FLT_MAX;
    float t_delta_x = FLT_MAX;
    float t_delta_y = FLT_MAX;
    float t_delta_z = FLT_MAX;

    if (step_x != 0) {
        float next_x = step_x > 0 ? (float)(map_x + 1) : (float)map_x;
        t_max_x = (next_x - origin[0]) / ray_direction[0];
        t_delta_x = 1.0f / fabsf(ray_direction[0]);
    }
    if (step_y != 0) {
        float next_y = step_y > 0 ? (float)(map_y + 1) : (float)map_y;
        t_max_y = (next_y - origin[1]) / ray_direction[1];
        t_delta_y = 1.0f / fabsf(ray_direction[1]);
    }
    if (step_z != 0) {
        float next_z = step_z > 0 ? (float)(map_z + 1) : (float)map_z;
        t_max_z = (next_z - origin[2]) / ray_direction[2];
        t_delta_z = 1.0f / fabsf(ray_direction[2]);
    }

    selection->hit = 0;
    selection->place_x = map_x;
    selection->place_y = map_y;
    selection->place_z = map_z;

    float travelled = 0.0f;
    while (travelled <= max_distance) {
        if (map_x >= 0 && map_x < CHUNK_SIZE_X &&
            map_y >= 0 && map_y < CHUNK_SIZE_Y &&
            map_z >= 0 && map_z < CHUNK_SIZE_Z) {
            Block* block = chunk_get_block(chunk, map_x, map_y, map_z);
            if (block != NULL && block_is_solid(block->type)) {
                selection->hit = 1;
                selection->block_x = map_x;
                selection->block_y = map_y;
                selection->block_z = map_z;
                return 1;
            }
        }

        if (t_max_x < t_max_y) {
            if (t_max_x < t_max_z) {
                travelled = t_max_x;
                if (travelled > max_distance) break;
                map_x += step_x;
                t_max_x += t_delta_x;
                selection->place_x = map_x - step_x;
                selection->place_y = map_y;
                selection->place_z = map_z;
            } else {
                travelled = t_max_z;
                if (travelled > max_distance) break;
                map_z += step_z;
                t_max_z += t_delta_z;
                selection->place_x = map_x;
                selection->place_y = map_y;
                selection->place_z = map_z - step_z;
            }
        } else {
            if (t_max_y < t_max_z) {
                travelled = t_max_y;
                if (travelled > max_distance) break;
                map_y += step_y;
                t_max_y += t_delta_y;
                selection->place_x = map_x;
                selection->place_y = map_y - step_y;
                selection->place_z = map_z;
            } else {
                travelled = t_max_z;
                if (travelled > max_distance) break;
                map_z += step_z;
                t_max_z += t_delta_z;
                selection->place_x = map_x;
                selection->place_y = map_y;
                selection->place_z = map_z - step_z;
            }
        }
    }

    return 0;
}

static int player_collides_at_h(Chunk* chunk, vec3 position, float ph) {
    const float half_width = 0.3f;
    float min_x = position[0] - half_width + 0.001f;
    float max_x = position[0] + half_width - 0.001f;
    float min_y = position[1]              + 0.001f;
    float max_y = position[1] + ph         - 0.001f;
    float min_z = position[2] - half_width + 0.001f;
    float max_z = position[2] + half_width - 0.001f;

    int start_x = (int)floorf(min_x);
    int end_x = (int)floorf(max_x);
    int start_y = (int)floorf(min_y);
    int end_y = (int)floorf(max_y);
    int start_z = (int)floorf(min_z);
    int end_z = (int)floorf(max_z);

    for (int x = start_x; x <= end_x; ++x) {
        for (int y = start_y; y <= end_y; ++y) {
            for (int z = start_z; z <= end_z; ++z) {
                Block* block = chunk_get_block(chunk, x, y, z);
                if (block != NULL && block_is_solid(block->type)) {
                    float block_min_x = (float)x;
                    float block_max_x = (float)x + 1.0f;
                    float block_min_y = (float)y;
                    float block_max_y = (float)y + 1.0f;
                    float block_min_z = (float)z;
                    float block_max_z = (float)z + 1.0f;

                    if (max_x >= block_min_x && min_x <= block_max_x &&
                        max_y >= block_min_y && min_y <= block_max_y &&
                        max_z >= block_min_z && min_z <= block_max_z) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

static int player_collides_at(Chunk* chunk, vec3 position) {
    return player_collides_at_h(chunk, position, 1.8f);
}

static int player_grounded_at_h(Chunk* chunk, vec3 position, float ph) {
    vec3 probe;
    glm_vec3_copy(position, probe);
    probe[1] -= 0.05f;
    return player_collides_at_h(chunk, probe, ph);
}

static int player_grounded_at(Chunk* chunk, vec3 position) {
    return player_grounded_at_h(chunk, position, 1.8f);
}

static void move_with_collision_h(Camera* cam, Chunk* chunk, vec3 movement, float ph, int edge_stop) {
    vec3 next;

    glm_vec3_copy(cam->position, next);
    next[1] += movement[1];
    if (!player_collides_at_h(chunk, next, ph))
        cam->position[1] = next[1];

    glm_vec3_copy(cam->position, next);
    next[0] += movement[0];
    if (!player_collides_at_h(chunk, next, ph)) {
        if (!edge_stop || player_grounded_at_h(chunk, next, ph))
            cam->position[0] = next[0];
    }

    glm_vec3_copy(cam->position, next);
    next[2] += movement[2];
    if (!player_collides_at_h(chunk, next, ph)) {
        if (!edge_stop || player_grounded_at_h(chunk, next, ph))
            cam->position[2] = next[2];
    }
}

static void move_camera_with_collision(Camera* cam, Chunk* chunk, vec3 movement) {
    move_with_collision_h(cam, chunk, movement, 1.8f, 0);
}

static const GLfloat selection_outline_vertices[] = {
    -0.01f, -0.01f, -0.01f,
     1.01f, -0.01f, -0.01f,
     1.01f,  1.01f, -0.01f,
    -0.01f,  1.01f, -0.01f,
    -0.01f, -0.01f,  1.01f,
     1.01f, -0.01f,  1.01f,
     1.01f,  1.01f,  1.01f,
    -0.01f,  1.01f,  1.01f,
};

static const GLuint selection_outline_indices[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    0, 4, 7, 0, 7, 3,
    1, 5, 6, 1, 6, 2,
    0, 1, 5, 0, 5, 4,
    3, 2, 6, 3, 6, 7,
};

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

    const char* selection_vert =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 camMatrix;\n"
        "uniform mat4 model;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = camMatrix * model * vec4(aPos, 1.0);\n"
        "}\n";

    const char* selection_frag =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec4 outlineColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = outlineColor;\n"
        "}\n";

    GLuint selectionProgram = create_program_from_sources(selection_vert, selection_frag);
    GLuint selectionVAO, selectionVBO, selectionEBO;
    glGenVertexArrays(1, &selectionVAO);
    glGenBuffers(1, &selectionVBO);
    glGenBuffers(1, &selectionEBO);
    glBindVertexArray(selectionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, selectionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(selection_outline_vertices), selection_outline_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selectionEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(selection_outline_indices), selection_outline_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

    const char* world_textures[] = {
        "src/textures/dirtblock.png",
        "src/textures/grass_side.png",
        "src/textures/grass_top.png",
        "src/textures/cobblestone.png",
    };
    GLuint texture = load_texture_array(world_textures, 4);

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
    vec3 start_pos = {0.0f, 4.0f, 10.0f};
    camera_init(&cam, width, height, start_pos);

    int running = 1;
    int paused = 0;
    SDL_Event event;
    int gravity_enabled = 0;


    const float WALK_SPEED       = 4.317f;
    const float SPRINT_SPEED     = 5.612f;
    const float CROUCH_SPEED     = 1.31f;
    const float FLY_SPEED        = 10.92f;
    const float FLY_SPRINT_SPEED = 21.6f;
    const float GRAVITY          = 28.0f;
    const float JUMP_IMPULSE     =  8.0f;
    const float FLOOR_Y          = -4.0f;
    const float HFRICTION_PS     = 0.03f;
    const float AIR_ACCEL_FRAC   = 0.2f;
    const Uint32 JUMP_BUFFER_MS  = 120;
    const float STAND_HEIGHT     = 1.8f;
    const float CROUCH_HEIGHT    = 1.35f;
    const float STAND_EYE        = 1.62f;
    const float CROUCH_EYE       = 1.27f;

    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    int   jump_buffer = 0;
    int   crouching = 0;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        int break_requested = 0;
        int place_requested = 0;

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
            } else if (!paused && event.type == SDL_KEYDOWN && event.key.repeat == 0
                       && event.key.keysym.sym == SDLK_SPACE && gravity_enabled) {
                jump_buffer = (int)JUMP_BUFFER_MS;
            } else if (paused && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int button_x = 0;
                int button_y = 0;
                int gravity_button_y = 0;
                int screen_w = 0;
                int screen_h = 0;
                SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);
                float button_width = 280.0f;
                float button_height = 64.0f;
                button_x = (screen_w - (int)button_width) / 2;
                button_y = (screen_h - (int)button_height) / 2;
                gravity_button_y = button_y + 84;

                if (point_in_rect(event.button.x, event.button.y, (float)button_x, (float)button_y, button_width, button_height)) {
                    paused = 0;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_ShowCursor(SDL_DISABLE);
                    cam.first_click = 1;
                } else if (point_in_rect(event.button.x, event.button.y, (float)button_x, (float)gravity_button_y, button_width, button_height)) {
                    gravity_enabled = !gravity_enabled;
                    if (!gravity_enabled) {
                        vel_y = 0.0f;
                    }
                }
            } else if (!paused && event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    break_requested = 1;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    place_requested = 1;
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

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_time) * 0.001f;
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;

        if (!paused) {
            int mouse_dx = 0, mouse_dy = 0;
            SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
            const Uint8* keys = SDL_GetKeyboardState(NULL);

            camera_rotate(&cam, mouse_dx, mouse_dy);

            vec3 wish_dir;
            int sprinting = 0;
            camera_get_wish_dir(&cam, keys, wish_dir, &sprinting);

            int want_crouch = (keys[SDL_SCANCODE_LSHIFT] && gravity_enabled) ? 1 : 0;

            if (want_crouch && !crouching) {
                crouching = 1;
            } else if (!want_crouch && crouching) {
                vec3 stand_test;
                glm_vec3_copy(cam.position, stand_test);
                if (!player_collides_at_h(&chunk, stand_test, STAND_HEIGHT))
                    crouching = 0;
            }

            float cur_height = crouching ? CROUCH_HEIGHT : STAND_HEIGHT;

            if (jump_buffer > 0)
                jump_buffer -= (int)(dt * 1000.0f);

            if (gravity_enabled) {
                int grounded = player_grounded_at_h(&chunk, cam.position, cur_height);

                if (crouching && !grounded) crouching = 0;
                cur_height = crouching ? CROUCH_HEIGHT : STAND_HEIGHT;

                float target_speed = crouching ? CROUCH_SPEED
                                   : (sprinting ? SPRINT_SPEED : WALK_SPEED);
                float accel_frac   = grounded ? 1.0f : AIR_ACCEL_FRAC;

                float wx = wish_dir[0] * target_speed;
                float wz = wish_dir[2] * target_speed;

                float accel = target_speed * 10.0f * accel_frac * dt;
                float dx_diff = wx - vel_x;
                float dz_diff = wz - vel_z;
                float diff_len = sqrtf(dx_diff*dx_diff + dz_diff*dz_diff);
                if (diff_len > 0.0001f) {
                    float step = accel;
                    if (step > diff_len) step = diff_len;
                    vel_x += (dx_diff / diff_len) * step;
                    vel_z += (dz_diff / diff_len) * step;
                }

                if (grounded) {
                    float friction = powf(HFRICTION_PS, dt);
                    if (wish_dir[0] == 0.0f && wish_dir[2] == 0.0f) {
                        vel_x *= friction;
                        vel_z *= friction;
                    }
                    if (vel_y < 0.0f) vel_y = 0.0f;
                }

                if (grounded && jump_buffer > 0 && !crouching) {
                    vel_y = JUMP_IMPULSE;
                    jump_buffer = 0;
                }

                vel_y -= GRAVITY * dt;

                vec3 move_xz = {vel_x * dt, 0.0f, vel_z * dt};
                move_with_collision_h(&cam, &chunk, move_xz, cur_height, crouching);

                vec3 next_pos;
                glm_vec3_copy(cam.position, next_pos);
                next_pos[1] += vel_y * dt;
                if (!player_collides_at_h(&chunk, next_pos, cur_height))
                    cam.position[1] = next_pos[1];
                else
                    vel_y = 0.0f;

                if (cam.position[1] < FLOOR_Y) {
                    cam.position[1] = FLOOR_Y;
                    vel_y = 0.0f;
                }

            } else {
                float fly_speed = sprinting ? FLY_SPRINT_SPEED : FLY_SPEED;

                vel_x = wish_dir[0] * fly_speed;
                vel_z = wish_dir[2] * fly_speed;
                vel_y = 0.0f;

                if (keys[SDL_SCANCODE_SPACE])  vel_y =  fly_speed;
                if (keys[SDL_SCANCODE_LSHIFT]) vel_y = -fly_speed;

                vec3 movement = {vel_x * dt, vel_y * dt, vel_z * dt};
                move_with_collision_h(&cam, &chunk, movement, cur_height, 0);
            }

            float eye_y = cam.position[1] + (crouching ? CROUCH_EYE : STAND_EYE);
            float real_y = cam.position[1];
            cam.position[1] = eye_y;

            BlockSelection selected_block = {0};
            raycast_block_selection(&chunk, cam.position, cam.orientation, 6.0f, &selected_block);

            if (break_requested && selected_block.hit) {
                if (set_block_type(&chunk, selected_block.block_x, selected_block.block_y, selected_block.block_z, BLOCK_AIR)) {
                    rebuild_chunk_mesh(&chunk, &chunk_mesh);
                    selected_block.hit = 0;
                    raycast_block_selection(&chunk, cam.position, cam.orientation, 6.0f, &selected_block);
                }
            }

            if (place_requested && selected_block.hit) {
                Block* place_block = chunk_get_block(&chunk, selected_block.place_x, selected_block.place_y, selected_block.place_z);
                if (place_block != NULL && place_block->type == BLOCK_AIR) {
                    place_block->type = BLOCK_COBBLESTONE;
                    vec3 foot_pos = {cam.position[0], real_y, cam.position[2]};
                    if (!player_collides_at_h(&chunk, foot_pos, cur_height)) {
                        rebuild_chunk_mesh(&chunk, &chunk_mesh);
                        selected_block.hit = 0;
                        raycast_block_selection(&chunk, cam.position, cam.orientation, 6.0f, &selected_block);
                    } else {
                        place_block->type = BLOCK_AIR;
                    }
                }
            }

            camera_update(&cam, 45.0f, 0.1f, 100.0f);
            cam.position[1] = real_y;
            camera_export(&cam, shaderProgram, "camMatrix");

            int modelLoc = glGetUniformLocation(shaderProgram, "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "tex0"), 0);
            mesh_draw(&chunk_mesh);

            if (selected_block.hit) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_CULL_FACE);
                glUseProgram(selectionProgram);
                glBindVertexArray(selectionVAO);

                mat4 selection_model;
                glm_mat4_identity(selection_model);
                vec3 selection_position = {(float)selected_block.block_x, (float)selected_block.block_y, (float)selected_block.block_z};
                glm_translate(selection_model, selection_position);

                glUniformMatrix4fv(glGetUniformLocation(selectionProgram, "camMatrix"), 1, GL_FALSE, (float*)cam.camera_matrix);
                glUniformMatrix4fv(glGetUniformLocation(selectionProgram, "model"), 1, GL_FALSE, (float*)selection_model);
                glUniform4f(glGetUniformLocation(selectionProgram, "outlineColor"), 0.55f, 0.55f, 0.55f, 0.35f);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

                glBindVertexArray(0);
                glEnable(GL_CULL_FACE);
                glDisable(GL_BLEND);
                glUseProgram(shaderProgram);
            }

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
            float paused_real_y = cam.position[1];
            cam.position[1] += crouching ? CROUCH_EYE : STAND_EYE;
            camera_update(&cam, 45.0f, 0.1f, 100.0f);
            cam.position[1] = paused_real_y;
            camera_export(&cam, shaderProgram, "camMatrix");

            int modelLoc = glGetUniformLocation(shaderProgram, "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (float*)model);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
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
            float gravity_button_y = button_y + 84.0f;

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

            float gravity_button_vertices[24] = {
                button_x, gravity_button_y, 0.0f, 0.0f,
                button_x + button_width, gravity_button_y, 1.0f, 0.0f,
                button_x + button_width, gravity_button_y + button_height, 1.0f, 1.0f,

                button_x, gravity_button_y, 0.0f, 0.0f,
                button_x + button_width, gravity_button_y + button_height, 1.0f, 1.0f,
                button_x, gravity_button_y + button_height, 0.0f, 1.0f,
            };
            glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(gravity_button_vertices), gravity_button_vertices);
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

            text_count = 0;
            const char* gravity_label = gravity_enabled ? "gravity on" : "toggle gravity";
            text_width = 0.0f;
            for (const char* ch = gravity_label; *ch != '\0'; ++ch) {
                if (*ch == ' ') {
                    text_width += 4.0f * text_scale;
                } else {
                    text_width += 6.0f * text_scale;
                }
            }
            text_x = button_x + (button_width - text_width) * 0.5f;
            text_y = gravity_button_y + (button_height - 7.0f * text_scale) * 0.5f;
            build_text_vertices(gravity_label, text_x, text_y, text_scale, text_vertices, &text_count);
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
    glDeleteProgram(selectionProgram);
    glDeleteBuffers(1, &uiVBO);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &buttonVBO);
    glDeleteVertexArrays(1, &buttonVAO);
    glDeleteBuffers(1, &selectionEBO);
    glDeleteBuffers(1, &selectionVBO);
    glDeleteVertexArrays(1, &selectionVAO);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}