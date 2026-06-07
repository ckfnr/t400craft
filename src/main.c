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
#include "World/block.h"
#include "World/chunk.h"
#include "World/world.h"
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
    int w = 0, h = 0;
    unsigned char* layer_data[layer_count];
    for (int i = 0; i < layer_count; ++i) {
        layer_data[i] = stbi_load(paths[i], &w, &h, NULL, 4);
        if (!layer_data[i]) {
            for (int j = 0; j < i; ++j) stbi_image_free(layer_data[j]);
            return 0;
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, w, h, layer_count, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    for (int i = 0; i < layer_count; ++i) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, layer_data[i]);
        stbi_image_free(layer_data[i]);
    }
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return tex;
}

static void append_rect(float* vertices, int* vertex_count, float x, float y, float rect_width, float rect_height) {
    float x2 = x + rect_width, y2 = y + rect_height;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y2;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y2;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y2;
}

static const unsigned char* glyph_rows(char c) {
    static const unsigned char blank[7] = {0,   0,   0,   0,   0,   0,   0  };
    static const unsigned char a_glyph[7] = {4,  10,  17,  17,  31,  17,  17 };
    static const unsigned char b_glyph[7] = {30, 17,  17,  30,  17,  17,  30 };
    static const unsigned char c_glyph[7] = {14, 17,  16,  16,  16,  17,  14 };
    static const unsigned char d_glyph[7] = {30, 17,  17,  17,  17,  17,  30 };
    static const unsigned char e_glyph[7] = {31, 16,  16,  30,  16,  16,  31 };
    static const unsigned char f_glyph[7] = {31, 16,  16,  30,  16,  16,  16 };
    static const unsigned char g_glyph[7] = {14, 17,  16,  23,  17,  17,  14 };
    static const unsigned char h_glyph[7] = {17, 17,  17,  31,  17,  17,  17 };
    static const unsigned char i_glyph[7] = {14, 4,   4,   4,   4,   4,   14 };
    static const unsigned char k_glyph[7] = {17, 18,  20,  28,  20,  18,  17 };
    static const unsigned char l_glyph[7] = {16, 16,  16,  16,  16,  16,  31 };
    static const unsigned char m_glyph[7] = {17, 27,  21,  21,  17,  17,  17 };
    static const unsigned char n_glyph[7] = {17, 25,  25,  21,  19,  19,  17 };
    static const unsigned char o_glyph[7] = {14, 17,  17,  17,  17,  17,  14 };
    static const unsigned char p_glyph[7] = {30, 17,  17,  30,  16,  16,  16 };
    static const unsigned char r_glyph[7] = {30, 17,  17,  30,  20,  18,  17 };
    static const unsigned char s_glyph[7] = {14, 17,  16,  14,  1,   17,  14 };
    static const unsigned char t_glyph[7] = {31, 4,   4,   4,   4,   4,   4  };
    static const unsigned char u_glyph[7] = {17, 17,  17,  17,  17,  17,  14 };
    static const unsigned char v_glyph[7] = {17, 17,  17,  17,  10,  10,  4  };
    static const unsigned char w_glyph[7] = {17, 17,  17,  21,  21,  27,  17 };
    static const unsigned char y_glyph[7] = {17, 17,  10,  4,   4,   4,   4  };
    static const unsigned char zero_glyph[7]  = {14, 17, 17, 17, 17, 17, 14};
    static const unsigned char one_glyph[7]   = { 4, 12,  4,  4,  4,  4, 14};
    static const unsigned char two_glyph[7]   = {14, 17,  1,  2,  4,  8, 31};
    static const unsigned char three_glyph[7] = {14, 17,  1,  6,  1, 17, 14};
    static const unsigned char four_glyph[7]  = {17, 17, 17, 31,  1,  1,  1};
    static const unsigned char five_glyph[7]  = {31, 16, 30,  1,  1, 17, 14};
    static const unsigned char six_glyph[7]   = {14, 16, 16, 30, 17, 17, 14};
    static const unsigned char seven_glyph[7] = {31,  1,  2,  4,  4,  4,  4};
    static const unsigned char eight_glyph[7] = {14, 17, 17, 14, 17, 17, 14};
    static const unsigned char nine_glyph[7]  = {14, 17, 17, 15,  1, 17, 14};
    switch (c) {
        case 'a': return a_glyph; case 'b': return b_glyph; case 'c': return c_glyph;
        case 'd': return d_glyph; case 'e': return e_glyph; case 'f': return f_glyph;
        case 'g': return g_glyph; case 'h': return h_glyph; case 'i': return i_glyph;
        case 'k': return k_glyph; case 'l': return l_glyph; case 'm': return m_glyph;
        case 'n': return n_glyph; case 'o': return o_glyph; case 'p': return p_glyph;
        case 'r': return r_glyph; case 's': return s_glyph; case 't': return t_glyph;
        case 'u': return u_glyph; case 'v': return v_glyph; case 'w': return w_glyph;
        case 'y': return y_glyph;
        case '0': return zero_glyph;  case '1': return one_glyph;
        case '2': return two_glyph;   case '3': return three_glyph;
        case '4': return four_glyph;  case '5': return five_glyph;
        case '6': return six_glyph;   case '7': return seven_glyph;
        case '8': return eight_glyph; case '9': return nine_glyph;
        default:  return blank;
    }
}

static void build_text_vertices(const char* text, float x, float y, float scale, float* vertices, int* vertex_count) {
    float cursor_x = x;
    for (const char* ch = text; *ch != '\0'; ++ch) {
        if (*ch == ' ') { cursor_x += 4.0f * scale; continue; }
        const unsigned char* rows = glyph_rows(*ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (rows[row] & (1 << (4 - col)))
                    append_rect(vertices, vertex_count, cursor_x + col * scale, y + row * scale, scale, scale);
        cursor_x += 6.0f * scale;
    }
}

static int point_in_rect(int px, int py, float x, float y, float rw, float rh) {
    return px >= (int)x && px <= (int)(x + rw) && py >= (int)y && py <= (int)(y + rh);
}

static int block_is_solid(BlockType type) { return type != BLOCK_AIR; }

typedef struct {
    int hit;
    int block_x, block_y, block_z;
    int place_x, place_y, place_z;
} BlockSelection;

typedef struct { float x, y, z, w; } FPlane;
static void extract_frustum(mat4 m, FPlane p[6]) {
    p[0] = (FPlane){m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0]};
    p[1] = (FPlane){m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0]};
    p[2] = (FPlane){m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1]};
    p[3] = (FPlane){m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1]};
    p[4] = (FPlane){m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2]};
    p[5] = (FPlane){m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2]};
}
static int chunk_in_frustum(FPlane p[6], float x0, float z0) {
    float x1 = x0 + CHUNK_SIZE_X, y1 = (float)CHUNK_SIZE_Y, z1 = z0 + CHUNK_SIZE_Z;
    for (int i = 0; i < 6; i++) {
        float px = p[i].x > 0 ? x1 : x0;
        float py = p[i].y > 0 ? y1 : 0.0f;
        float pz = p[i].z > 0 ? z1 : z0;
        if (p[i].x*px + p[i].y*py + p[i].z*pz + p[i].w < 0) return 0;
    }
    return 1;
}

static int player_collides_at_h(World* world, vec3 position, float ph) {
    const float hw = 0.3f;
    float min_x = position[0] - hw + 0.001f, max_x = position[0] + hw - 0.001f;
    float min_y = position[1]      + 0.001f, max_y = position[1] + ph - 0.001f;
    float min_z = position[2] - hw + 0.001f, max_z = position[2] + hw - 0.001f;
    int sx = (int)floorf(min_x), ex = (int)floorf(max_x);
    int sy = (int)floorf(min_y), ey = (int)floorf(max_y);
    int sz = (int)floorf(min_z), ez = (int)floorf(max_z);
    for (int x = sx; x <= ex; ++x)
        for (int y = sy; y <= ey; ++y)
            for (int z = sz; z <= ez; ++z) {
                Block* b = world_get_block(world, x, y, z);
                if (b && block_is_solid(b->type)) {
                    if (max_x >= (float)x   && min_x <= (float)x+1 &&
                        max_y >= (float)y   && min_y <= (float)y+1 &&
                        max_z >= (float)z   && min_z <= (float)z+1)
                        return 1;
                }
            }
    return 0;
}

static int player_grounded_at_h(World* world, vec3 position, float ph) {
    vec3 probe; glm_vec3_copy(position, probe); probe[1] -= 0.05f;
    return player_collides_at_h(world, probe, ph);
}

static void move_with_collision_h(Camera* cam, World* world, vec3 movement, float ph, int edge_stop) {
    vec3 next;
    glm_vec3_copy(cam->position, next); next[1] += movement[1];
    if (!player_collides_at_h(world, next, ph)) cam->position[1] = next[1];
    glm_vec3_copy(cam->position, next); next[0] += movement[0];
    if (!player_collides_at_h(world, next, ph))
        if (!edge_stop || player_grounded_at_h(world, next, ph))
            cam->position[0] = next[0];
    glm_vec3_copy(cam->position, next); next[2] += movement[2];
    if (!player_collides_at_h(world, next, ph))
        if (!edge_stop || player_grounded_at_h(world, next, ph))
            cam->position[2] = next[2];
}

static int raycast_block_selection(World* world, vec3 origin, vec3 direction, float max_dist, BlockSelection* sel) {
    vec3 rd; glm_vec3_copy(direction, rd);
    if (glm_vec3_norm(rd) < 0.0001f) return 0;
    glm_vec3_normalize(rd);
    int mx = (int)floorf(origin[0]), my = (int)floorf(origin[1]), mz = (int)floorf(origin[2]);
    int sx = rd[0] > 0 ? 1 : (rd[0] < 0 ? -1 : 0);
    int sy = rd[1] > 0 ? 1 : (rd[1] < 0 ? -1 : 0);
    int sz = rd[2] > 0 ? 1 : (rd[2] < 0 ? -1 : 0);
    float tmx=FLT_MAX, tmy=FLT_MAX, tmz=FLT_MAX, tdx=FLT_MAX, tdy=FLT_MAX, tdz=FLT_MAX;
    if (sx) { tmx = ((sx>0?(float)(mx+1):(float)mx)-origin[0])/rd[0]; tdx=1.0f/fabsf(rd[0]); }
    if (sy) { tmy = ((sy>0?(float)(my+1):(float)my)-origin[1])/rd[1]; tdy=1.0f/fabsf(rd[1]); }
    if (sz) { tmz = ((sz>0?(float)(mz+1):(float)mz)-origin[2])/rd[2]; tdz=1.0f/fabsf(rd[2]); }
    sel->hit=0; sel->place_x=mx; sel->place_y=my; sel->place_z=mz;
    float travelled = 0.0f;
    while (travelled <= max_dist) {
        Block* b = world_get_block(world, mx, my, mz);
        if (b && block_is_solid(b->type)) {
            sel->hit=1; sel->block_x=mx; sel->block_y=my; sel->block_z=mz; return 1;
        }
        if (tmx < tmy) {
            if (tmx < tmz) { travelled=tmx; if(travelled>max_dist)break; mx+=sx; tmx+=tdx; sel->place_x=mx-sx; sel->place_y=my; sel->place_z=mz; }
            else           { travelled=tmz; if(travelled>max_dist)break; mz+=sz; tmz+=tdz; sel->place_x=mx; sel->place_y=my; sel->place_z=mz-sz; }
        } else {
            if (tmy < tmz) { travelled=tmy; if(travelled>max_dist)break; my+=sy; tmy+=tdy; sel->place_x=mx; sel->place_y=my-sy; sel->place_z=mz; }
            else           { travelled=tmz; if(travelled>max_dist)break; mz+=sz; tmz+=tdz; sel->place_x=mx; sel->place_y=my; sel->place_z=mz-sz; }
        }
    }
    return 0;
}

static const GLfloat selection_outline_vertices[] = {
    -0.01f,-0.01f,-0.01f,  1.01f,-0.01f,-0.01f,  1.01f, 1.01f,-0.01f, -0.01f, 1.01f,-0.01f,
    -0.01f,-0.01f, 1.01f,  1.01f,-0.01f, 1.01f,  1.01f, 1.01f, 1.01f, -0.01f, 1.01f, 1.01f,
};
static const GLuint selection_outline_indices[] = {
    0,1,2, 0,2,3,  4,5,6, 4,6,7,  0,4,7, 0,7,3,
    1,5,6, 1,6,2,  0,1,5, 0,5,4,  3,2,6, 3,6,7,
};

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("t400craft",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_GL_SetSwapInterval(0);
    glewInit();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    char* vertSrc = load_file("src/Shaders/default.vert");
    char* fragSrc = load_file("src/Shaders/default.frag");
    GLuint vertexShader   = compile_shader_source(GL_VERTEX_SHADER,   vertSrc);
    GLuint fragmentShader = compile_shader_source(GL_FRAGMENT_SHADER, fragSrc);
    free(vertSrc); free(fragSrc);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glBindAttribLocation(shaderProgram, 0, "aPos");
    glBindAttribLocation(shaderProgram, 1, "aColor");
    glBindAttribLocation(shaderProgram, 2, "aTexCoord");
    glBindAttribLocation(shaderProgram, 3, "aTexLayer");
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint u_camMatrix  = glGetUniformLocation(shaderProgram, "camMatrix");
    GLint u_view       = glGetUniformLocation(shaderProgram, "view");
    GLint u_proj       = glGetUniformLocation(shaderProgram, "proj");
    GLint u_fog_color  = glGetUniformLocation(shaderProgram, "fog_color");
    GLint u_fog_start  = glGetUniformLocation(shaderProgram, "fog_start");
    GLint u_fog_end    = glGetUniformLocation(shaderProgram, "fog_end");
    GLint u_tex0       = glGetUniformLocation(shaderProgram, "tex0");
    GLint u_model      = glGetUniformLocation(shaderProgram, "model");

    const char* ui_vert =
        "#version 120\n"
        "attribute vec2 aPos;\n"
        "uniform vec2 screenSize;\n"
        "void main() {\n"
        "   vec2 ndc = vec2((aPos.x/screenSize.x)*2.0-1.0, 1.0-(aPos.y/screenSize.y)*2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";
    const char* ui_frag =
        "#version 120\n"
        "uniform vec4 uColor;\n"
        "void main() { gl_FragColor = uColor; }\n";
    GLuint uiProgram = create_program_from_sources(ui_vert, ui_frag);
    GLint u_ui_screenSize = glGetUniformLocation(uiProgram, "screenSize");
    GLint u_ui_color      = glGetUniformLocation(uiProgram, "uColor");

    GLuint uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*32768, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    const char* button_vert =
        "#version 120\n"
        "attribute vec2 aPos;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 texCoord;\n"
        "uniform vec2 screenSize;\n"
        "void main() {\n"
        "   vec2 ndc = vec2((aPos.x/screenSize.x)*2.0-1.0, 1.0-(aPos.y/screenSize.y)*2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0); texCoord = aTexCoord;\n"
        "}\n";
    const char* button_frag =
        "#version 120\n"
        "varying vec2 texCoord;\n"
        "uniform sampler2D buttonTex;\n"
        "void main() { gl_FragColor = texture2D(buttonTex, texCoord); }\n";
    GLuint buttonProgram = create_program_from_sources(button_vert, button_frag);
    GLint u_btn_screenSize = glGetUniformLocation(buttonProgram, "screenSize");
    GLint u_btn_tex        = glGetUniformLocation(buttonProgram, "buttonTex");

    const char* selection_vert =
        "#version 120\n"
        "attribute vec3 aPos;\n"
        "uniform mat4 camMatrix;\n"
        "uniform mat4 model;\n"
        "void main() { gl_Position = camMatrix * model * vec4(aPos, 1.0); }\n";
    const char* selection_frag =
        "#version 120\n"
        "uniform vec4 outlineColor;\n"
        "void main() { gl_FragColor = outlineColor; }\n";
    GLuint selectionProgram = create_program_from_sources(selection_vert, selection_frag);
    GLint u_sel_cam   = glGetUniformLocation(selectionProgram, "camMatrix");
    GLint u_sel_model = glGetUniformLocation(selectionProgram, "model");
    GLint u_sel_color = glGetUniformLocation(selectionProgram, "outlineColor");

    GLuint selectionVAO, selectionVBO, selectionEBO;
    glGenVertexArrays(1, &selectionVAO); glGenBuffers(1, &selectionVBO); glGenBuffers(1, &selectionEBO);
    glBindVertexArray(selectionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, selectionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(selection_outline_vertices), selection_outline_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selectionEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(selection_outline_indices), selection_outline_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GLuint buttonVAO, buttonVBO;
    glGenVertexArrays(1, &buttonVAO); glGenBuffers(1, &buttonVBO);
    glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*24, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float))); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    World* world = malloc(sizeof(World));
    if (!world) { SDL_Quit(); return 1; }
    world_init(world, 0, 0, "Savefiles");

    const char* world_textures[] = {
        "src/textures/dirtblock.png",
        "src/textures/grass_side.png",
        "src/textures/grass_top.png",
        "src/textures/cobblestone.png",
    };
    GLuint texture = load_texture_array(world_textures, 4);

    int buttonW, buttonH, buttonCh;
    unsigned char* buttonBytes = stbi_load("src/UI/button.png", &buttonW, &buttonH, &buttonCh, 4);
    GLuint buttonTexture;
    glGenTextures(1, &buttonTexture);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, buttonTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buttonW, buttonH, 0, GL_RGBA, GL_UNSIGNED_BYTE, buttonBytes);
    glGenerateMipmap(GL_TEXTURE_2D); stbi_image_free(buttonBytes); glBindTexture(GL_TEXTURE_2D, 0);

    int crossW, crossH, crossCh;
    unsigned char* crossBytes = stbi_load("src/UI/crosshair.png", &crossW, &crossH, &crossCh, 4);
    GLuint crosshairTexture;
    glGenTextures(1, &crosshairTexture);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, crosshairTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, crossW, crossH, 0, GL_RGBA, GL_UNSIGNED_BYTE, crossBytes);
    stbi_image_free(crossBytes); glBindTexture(GL_TEXTURE_2D, 0);

    int spawn_wx = 8, spawn_wz = 8, spawn_y = 85;
    {
        Chunk* sc = world_get_chunk(world, 0, 0);
        if (sc) {
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                Block* b = chunk_get_block(sc, spawn_wx, y, spawn_wz);
                if (b && block_is_solid(b->type)) { spawn_y = y + 1; break; }
            }
        }
    }

    Camera cam;
    vec3 start_pos = {(float)spawn_wx + 0.5f, (float)spawn_y, (float)spawn_wz + 0.5f};
    camera_init(&cam, width, height, start_pos);

    {
        FILE* pf = fopen("Savefiles/player.bin", "rb");
        if (pf) {
            float px, py, pz;
            if (fread(&px,sizeof(float),1,pf)==1 &&
                fread(&py,sizeof(float),1,pf)==1 &&
                fread(&pz,sizeof(float),1,pf)==1) {
                cam.position[0]=px; cam.position[1]=py; cam.position[2]=pz;
            }
            fclose(pf);
        }
    }

    int running = 1, paused = 0;
    SDL_Event event;
    int gravity_enabled = 1;
    int dynamic_lighting = 0;

    const float WALK_SPEED=4.317f, SPRINT_SPEED=7.5f, CROUCH_SPEED=1.31f;
    const float FLY_SPEED=10.92f, FLY_SPRINT_SPEED=21.6f;
    const float GRAVITY=28.0f, JUMP_IMPULSE=8.9f, FLOOR_Y=-4.0f;
    const float HFRICTION_PS=0.03f, AIR_ACCEL_FRAC=0.2f;
    const float STAND_HEIGHT=1.8f, CROUCH_HEIGHT=1.35f;
    const float STAND_EYE=1.62f, CROUCH_EYE=1.27f;
    const float FOG_START=3.0f*16.0f, FOG_END=4.5f*16.0f;
    const float FOG_R=0.4f, FOG_G=0.7f, FOG_B=1.0f;

    float vel_x=0, vel_y=0, vel_z=0;
    int jump_buffer=0, crouching=0;
    int mouse_right_held=0;
    float place_timer=0.0f;
    int show_fps=0, fps_count=0, fps_display=0;
    float fps_timer=0.0f;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        int break_requested=0, place_requested=0;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_F3)
                show_fps = !show_fps;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                paused = !paused;
                if (paused) { SDL_SetRelativeMouseMode(SDL_FALSE); SDL_ShowCursor(SDL_ENABLE); }
                else { SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click = 1; }
            } else if (!paused && event.type == SDL_KEYDOWN && event.key.repeat == 0
                       && event.key.keysym.sym == SDLK_SPACE && gravity_enabled) {
                (void)0;
            } else if (paused && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                float bw=280.0f, bh=64.0f;
                float bx=((float)sw-bw)*0.5f, by=((float)sh-bh)*0.5f;
                float gby=by+84.0f, lby=by+168.0f;
                if (point_in_rect(event.button.x, event.button.y, bx, by, bw, bh)) {
                    paused=0; SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click=1;
                } else if (point_in_rect(event.button.x, event.button.y, bx, gby, bw, bh)) {
                    gravity_enabled=!gravity_enabled; if(!gravity_enabled) vel_y=0.0f;
                } else if (point_in_rect(event.button.x, event.button.y, bx, lby, bw, bh)) {
                    dynamic_lighting=!dynamic_lighting;
                    world->dynamic_lighting=dynamic_lighting;
                    for(int ri=0;ri<WORLD_SLOTS;ri++)
                        if(world->slots[ri].loaded) world->slots[ri].mesh_valid=0;
                }
            } else if (!paused && event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT)       break_requested=1;
                if (event.button.button == SDL_BUTTON_RIGHT)      mouse_right_held=1;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_RIGHT)      mouse_right_held=0;
            }
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w,h; SDL_GL_GetDrawableSize(window,&w,&h);
                glViewport(0,0,w,h); cam.width=w; cam.height=h;
            }
        }

        int cx_player = (int)floorf(cam.position[0] / CHUNK_SIZE_X);
        int cz_player = (int)floorf(cam.position[2] / CHUNK_SIZE_Z);
        world_update_center(world, cx_player, cz_player);

        {
            int rebuilt = 0;
            for (int i = 0; i < WORLD_SLOTS && rebuilt < 1; i++) {
                WorldSlot* s = &world->slots[i];
                if (s->loaded && !s->mesh_valid) {
                    world_rebuild_mesh(world, s->chunk.cx, s->chunk.cz);
                    rebuilt++;
                }
            }
        }

        int screen_w=0, screen_h=0;
        SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);

        glClearColor(FOG_R, FOG_G, FOG_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        mat4 model; glm_mat4_identity(model);

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_time) * 0.001f;
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;

        fps_count++;
        fps_timer += dt;
        if (fps_timer >= 0.5f) {
            fps_display = (int)(fps_count / fps_timer + 0.5f);
            fps_count=0; fps_timer=0.0f;
        }

        place_timer += dt;
        if (mouse_right_held && place_timer >= 0.2f) {
            place_requested = 1;
            place_timer = 0.0f;
        }

        if (!paused) {
            int mouse_dx=0, mouse_dy=0;
            SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            camera_rotate(&cam, mouse_dx, mouse_dy);

            vec3 wish_dir; int sprinting=0;
            camera_get_wish_dir(&cam, keys, wish_dir, &sprinting);

            int want_crouch = (keys[SDL_SCANCODE_LSHIFT] && gravity_enabled) ? 1 : 0;
            if (want_crouch && !crouching) crouching=1;
            else if (!want_crouch && crouching) {
                vec3 st; glm_vec3_copy(cam.position, st);
                if (!player_collides_at_h(world, st, STAND_HEIGHT)) crouching=0;
            }
            float cur_height = crouching ? CROUCH_HEIGHT : STAND_HEIGHT;
            if (jump_buffer > 0) jump_buffer -= (int)(dt * 1000.0f);

            if (gravity_enabled) {
                int grounded = player_grounded_at_h(world, cam.position, cur_height);
                if (crouching && !grounded) { crouching=0; cur_height=STAND_HEIGHT; }
                float target_speed = crouching ? CROUCH_SPEED : (sprinting ? SPRINT_SPEED : WALK_SPEED);
                float accel_frac = grounded ? 1.0f : AIR_ACCEL_FRAC;
                float wx=wish_dir[0]*target_speed, wz=wish_dir[2]*target_speed;
                float accel=target_speed*10.0f*accel_frac*dt;
                float ddx=wx-vel_x, ddz=wz-vel_z;
                float dlen=sqrtf(ddx*ddx+ddz*ddz);
                if (dlen > 0.0001f) { float step=accel<dlen?accel:dlen; vel_x+=(ddx/dlen)*step; vel_z+=(ddz/dlen)*step; }
                if (grounded) {
                    float friction=powf(HFRICTION_PS,dt);
                    if(wish_dir[0]==0.0f&&wish_dir[2]==0.0f){vel_x*=friction;vel_z*=friction;}
                    if(vel_y<0.0f) vel_y=0.0f;
                }
                if (grounded && keys[SDL_SCANCODE_SPACE] && !crouching) { vel_y=JUMP_IMPULSE; }
                vel_y -= GRAVITY*dt;
                vec3 move_xz = {vel_x*dt, 0.0f, vel_z*dt};
                move_with_collision_h(&cam, world, move_xz, cur_height, crouching);
                vec3 next_pos; glm_vec3_copy(cam.position, next_pos);
                next_pos[1] += vel_y*dt;
                if (!player_collides_at_h(world, next_pos, cur_height)) cam.position[1]=next_pos[1];
                else vel_y=0.0f;
                if (cam.position[1] < FLOOR_Y) { cam.position[1]=FLOOR_Y; vel_y=0.0f; }
            } else {
                float fly_speed=sprinting?FLY_SPRINT_SPEED:FLY_SPEED;
                vel_x=wish_dir[0]*fly_speed; vel_z=wish_dir[2]*fly_speed; vel_y=0.0f;
                if(keys[SDL_SCANCODE_SPACE])  vel_y= fly_speed;
                if(keys[SDL_SCANCODE_LSHIFT]) vel_y=-fly_speed;
                vec3 movement={vel_x*dt,vel_y*dt,vel_z*dt};
                move_with_collision_h(&cam, world, movement, cur_height, 0);
            }

            float eye_y = cam.position[1] + (crouching ? CROUCH_EYE : STAND_EYE);
            float real_y = cam.position[1];
            cam.position[1] = eye_y;

            BlockSelection sel = {0};
            raycast_block_selection(world, cam.position, cam.orientation, 6.0f, &sel);

            if (break_requested && sel.hit) {
                if (world_set_block(world, sel.block_x, sel.block_y, sel.block_z, BLOCK_AIR)) {
                    sel.hit=0; raycast_block_selection(world, cam.position, cam.orientation, 6.0f, &sel);
                }
            }
            if (place_requested && sel.hit) {
                Block* pb = world_get_block(world, sel.place_x, sel.place_y, sel.place_z);
                if (pb && pb->type == BLOCK_AIR) {
                    pb->type = BLOCK_COBBLESTONE;
                    vec3 foot = {cam.position[0], real_y, cam.position[2]};
                    if (!player_collides_at_h(world, foot, cur_height)) {
                        int pcx=(int)floorf((float)sel.place_x/CHUNK_SIZE_X);
                        int pcz=(int)floorf((float)sel.place_z/CHUNK_SIZE_Z);
                        WorldSlot* ps=world_get_slot(world,pcx,pcz);
                        if(ps){ps->chunk.dirty=1;world_rebuild_mesh(world,pcx,pcz);}
                        sel.hit=0; raycast_block_selection(world,cam.position,cam.orientation,6.0f,&sel);
                    } else { pb->type=BLOCK_AIR; }
                }
            }

            camera_update(&cam, 45.0f, 0.1f, 1000.0f);
            cam.position[1] = real_y;

            glUniformMatrix4fv(u_camMatrix, 1, GL_FALSE, (float*)cam.camera_matrix);
            glUniformMatrix4fv(u_view,      1, GL_FALSE, (float*)cam.view);
            glUniformMatrix4fv(u_proj,      1, GL_FALSE, (float*)cam.proj);
            glUniform3f(u_fog_color, FOG_R, FOG_G, FOG_B);
            glUniform1f(u_fog_start, FOG_START);
            glUniform1f(u_fog_end,   FOG_END);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glUniform1i(u_tex0, 0);

            FPlane frustum[6];
            extract_frustum(cam.camera_matrix, frustum);

            for (int i = 0; i < WORLD_SLOTS; i++) {
                WorldSlot* s = &world->slots[i];
                if (!s->loaded || !s->mesh_valid) continue;
                float wx = (float)(s->chunk.cx * CHUNK_SIZE_X);
                float wz = (float)(s->chunk.cz * CHUNK_SIZE_Z);
                if (!chunk_in_frustum(frustum, wx, wz)) continue;
                mat4 chunk_model; glm_mat4_identity(chunk_model);
                vec3 chunk_offset = {wx, 0.0f, wz};
                glm_translate(chunk_model, chunk_offset);
                glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)chunk_model);
                mesh_draw(&s->mesh);
            }

            if (sel.hit) {
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_CULL_FACE);
                glUseProgram(selectionProgram);
                glBindVertexArray(selectionVAO);
                mat4 sel_model; glm_mat4_identity(sel_model);
                vec3 sel_pos = {(float)sel.block_x, (float)sel.block_y, (float)sel.block_z};
                glm_translate(sel_model, sel_pos);
                glUniformMatrix4fv(u_sel_cam,   1, GL_FALSE, (float*)cam.camera_matrix);
                glUniformMatrix4fv(u_sel_model, 1, GL_FALSE, (float*)sel_model);
                glUniform4f(u_sel_color, 0.55f, 0.55f, 0.55f, 0.35f);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0); glEnable(GL_CULL_FACE); glDisable(GL_BLEND);
                glUseProgram(shaderProgram);
            }

            glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            float cx2=(float)screen_w*0.5f, cy2=(float)screen_h*0.5f, cw=32.0f, ch2=32.0f;
            float crosshair_vertices[24] = {
                cx2-cw*0.5f,cy2-ch2*0.5f,0,0, cx2+cw*0.5f,cy2-ch2*0.5f,1,0,
                cx2+cw*0.5f,cy2+ch2*0.5f,1,1, cx2-cw*0.5f,cy2-ch2*0.5f,0,0,
                cx2+cw*0.5f,cy2+ch2*0.5f,1,1, cx2-cw*0.5f,cy2+ch2*0.5f,0,1,
            };
            glUseProgram(buttonProgram); glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER,buttonVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(crosshair_vertices), crosshair_vertices);
            glUniform2f(u_btn_screenSize, (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, crosshairTexture);
            glUniform1i(u_btn_tex, 2);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindBuffer(GL_ARRAY_BUFFER,0); glBindVertexArray(0);
            glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);

            if (show_fps) {
                glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
                char fps_str[16]; int n=fps_display, pos=0;
                if(n==0){fps_str[pos++]='0';}
                else{char tmp[8];int tl=0;while(n>0){tmp[tl++]=(char)('0'+n%10);n/=10;}for(int fi=tl-1;fi>=0;fi--)fps_str[pos++]=tmp[fi];}
                fps_str[pos++]=' ';fps_str[pos++]='f';fps_str[pos++]='p';fps_str[pos++]='s';fps_str[pos]=0;
                float ftverts[4096]; int ftc=0;
                build_text_vertices(fps_str,8,8,2.5f,ftverts,&ftc);
                glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(float)*ftc,ftverts);
                glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
                glUniform4f(u_ui_color,1,1,1,1);
                glDrawArrays(GL_TRIANGLES,0,ftc/2);
                glBindBuffer(GL_ARRAY_BUFFER,0); glBindVertexArray(0);
                glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
            }

        } else {
            float pry=cam.position[1];
            cam.position[1]+=crouching?CROUCH_EYE:STAND_EYE;
            camera_update(&cam,45.0f,0.1f,1000.0f);
            cam.position[1]=pry;
            glUniformMatrix4fv(u_camMatrix,1,GL_FALSE,(float*)cam.camera_matrix);
            glUniformMatrix4fv(u_view,1,GL_FALSE,(float*)cam.view);
            glUniformMatrix4fv(u_proj,1,GL_FALSE,(float*)cam.proj);
            glUniform3f(u_fog_color,FOG_R,FOG_G,FOG_B);
            glUniform1f(u_fog_start,FOG_START); glUniform1f(u_fog_end,FOG_END);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
            glUniform1i(u_tex0,0);

            FPlane frustum[6]; extract_frustum(cam.camera_matrix, frustum);
            for (int i=0;i<WORLD_SLOTS;i++) {
                WorldSlot* s=&world->slots[i];
                if(!s->loaded||!s->mesh_valid) continue;
                float wx=(float)(s->chunk.cx*CHUNK_SIZE_X), wz=(float)(s->chunk.cz*CHUNK_SIZE_Z);
                if(!chunk_in_frustum(frustum,wx,wz)) continue;
                mat4 cm; glm_mat4_identity(cm);
                vec3 co={wx,0.0f,wz}; glm_translate(cm,co);
                glUniformMatrix4fv(u_model,1,GL_FALSE,(float*)cm);
                mesh_draw(&s->mesh);
            }

            glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
            float overlay[12]; int oc=0;
            append_rect(overlay,&oc,0,0,(float)screen_w,(float)screen_h);
            glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(float)*oc,overlay);
            glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
            glUniform4f(u_ui_color,0.35f,0.35f,0.35f,0.65f);
            glDrawArrays(GL_TRIANGLES,0,6);

            float bw=280,bh=64,bx=((float)screen_w-bw)*0.5f,by=((float)screen_h-bh)*0.5f,gby=by+84,lby=by+168;
            glUseProgram(buttonProgram); glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER,buttonVBO);
            glUniform2f(u_btn_screenSize,(float)screen_w,(float)screen_h);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,buttonTexture);
            glUniform1i(u_btn_tex,1);
            float boffsets[]={0,84,168};
            for(int b=0;b<3;b++){
                float by2=by+boffsets[b];
                float bv[24]={bx,by2,0,0, bx+bw,by2,1,0, bx+bw,by2+bh,1,1, bx,by2,0,0, bx+bw,by2+bh,1,1, bx,by2+bh,0,1};
                glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(bv),bv);
                glDrawArrays(GL_TRIANGLES,0,6);
            }

            glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
            float tverts[32768]; int tc=0; float ts=2.2f;
            const char* lbls[]={ "continue playing",
                                  gravity_enabled?"gravity on":"gravity off",
                                  dynamic_lighting?"lighting dynamic":"lighting flat" };
            float ypos[]={by,gby,lby};
            for(int b=0;b<3;b++){
                tc=0; float tw=0;
                for(const char* ch=lbls[b];*ch;++ch) tw+=(*ch==' ')?4.0f*ts:6.0f*ts;
                build_text_vertices(lbls[b],bx+(bw-tw)*0.5f,ypos[b]+(bh-7*ts)*0.5f,ts,tverts,&tc);
                glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(float)*tc,tverts);
                glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
                glUniform4f(u_ui_color,0.15f,0.15f,0.15f,1);
                glDrawArrays(GL_TRIANGLES,0,tc/2);
            }
            glBindBuffer(GL_ARRAY_BUFFER,0); glBindVertexArray(0);
            glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
        }

        SDL_GL_SwapWindow(window);
    }

    {
        FILE* pf=fopen("Savefiles/player.bin","wb");
        if(pf){
            fwrite(&cam.position[0],sizeof(float),1,pf);
            fwrite(&cam.position[1],sizeof(float),1,pf);
            fwrite(&cam.position[2],sizeof(float),1,pf);
            fclose(pf);
        }
    }

    world_save_all_dirty(world);
    world_free(world);
    glDeleteTextures(1,&texture); glDeleteTextures(1,&buttonTexture); glDeleteTextures(1,&crosshairTexture);
    glDeleteProgram(shaderProgram); glDeleteProgram(uiProgram); glDeleteProgram(buttonProgram); glDeleteProgram(selectionProgram);
    glDeleteBuffers(1,&uiVBO); glDeleteVertexArrays(1,&uiVAO);
    glDeleteBuffers(1,&buttonVBO); glDeleteVertexArrays(1,&buttonVAO);
    glDeleteBuffers(1,&selectionEBO); glDeleteBuffers(1,&selectionVBO); glDeleteVertexArrays(1,&selectionVAO);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(world);
    return 0;
}