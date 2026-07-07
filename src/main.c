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

static int block_is_solid(BlockType type) { return block_opaque(type); }

typedef struct {
    const char* tex_path;
    BlockType block;
    int is_bucket;
} ItemDef;

#define ITEM_COUNT 9
static const ItemDef item_defs[ITEM_COUNT] = {
    {NULL,                            BLOCK_AIR,         0},
    {"src/textures/cobblestone.png",  BLOCK_COBBLESTONE, 0},
    {"src/textures/oak_planks.png",   BLOCK_OAK_PLANKS,  0},
    {"src/textures/grass_side.png",   BLOCK_GRASS,       0},
    {"src/textures/dirtblock.png",    BLOCK_DIRT,        0},
    {"src/textures/water_bucket.png", BLOCK_AIR,         1},
    {"src/textures/oaklog_side.png",  BLOCK_OAK_LOG,     0},
    {"src/textures/oak_leaves.png",   BLOCK_OAK_LEAVES,  0},
    {"src/textures/glass_block.png",  BLOCK_GLASS,       0},
};

#define INV_SIZE 36
#define INV_MAIN_COUNT 27
#define INV_HOTBAR_START 27

typedef struct { float x, y, cell, pad, gap; } InvLayout;

static void inventory_layout(int screen_w, int screen_h, InvLayout* L) {
    float cell = (float)screen_h * 0.07f;
    if (cell < 20.0f) cell = 20.0f;
    if (cell > 64.0f) cell = 64.0f;
    L->cell = cell;
    L->pad = cell * 0.1f;
    L->gap = cell * 0.6f;
    float grid_w = 9.0f * cell + 8.0f * L->pad;
    float grid_h = 4.0f * cell + 3.0f * L->pad + L->gap;
    L->x = ((float)screen_w - grid_w) * 0.5f;
    L->y = ((float)screen_h - grid_h) * 0.5f;
}

static void inventory_slot_rect(const InvLayout* L, int slot, float* rx, float* ry) {
    if (slot < INV_MAIN_COUNT) {
        *rx = L->x + (float)(slot % 9) * (L->cell + L->pad);
        *ry = L->y + (float)(slot / 9) * (L->cell + L->pad);
    } else {
        *rx = L->x + (float)(slot - INV_HOTBAR_START) * (L->cell + L->pad);
        *ry = L->y + 3.0f * (L->cell + L->pad) + L->gap;
    }
}

static int inventory_slot_at(const InvLayout* L, float mx, float my) {
    for (int i = 0; i < INV_SIZE; i++) {
        float rx, ry;
        inventory_slot_rect(L, i, &rx, &ry);
        if (mx >= rx && mx <= rx + L->cell && my >= ry && my <= ry + L->cell) return i;
    }
    return -1;
}

static void inventory_shift_click(int* inv, int slot) {
    if (inv[slot] == 0) return;
    int start = slot < INV_MAIN_COUNT ? INV_HOTBAR_START : 0;
    int end   = slot < INV_MAIN_COUNT ? INV_SIZE : INV_MAIN_COUNT;
    for (int i = start; i < end; i++)
        if (inv[i] == 0) {
            inv[i] = inv[slot];
            inv[slot] = 0;
            return;
        }
}

static void inventory_put_back(int* inv, int* drag_item, int* drag_from) {
    if (*drag_item) {
        if (*drag_from >= 0 && inv[*drag_from] == 0) {
            inv[*drag_from] = *drag_item;
        } else {
            for (int i = 0; i < INV_SIZE; i++)
                if (inv[i] == 0) { inv[i] = *drag_item; break; }
        }
    }
    *drag_item = 0;
    *drag_from = -1;
}

static void load_inventory(const char* path, int* inv) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    uint8_t buf[INV_SIZE];
    if (fread(buf, 1, INV_SIZE, f) == INV_SIZE)
        for (int i = 0; i < INV_SIZE; i++)
            inv[i] = buf[i] < ITEM_COUNT ? buf[i] : 0;
    fclose(f);
}

static void save_inventory(const char* path, const int* inv) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    uint8_t buf[INV_SIZE];
    for (int i = 0; i < INV_SIZE; i++)
        buf[i] = (uint8_t)inv[i];
    fwrite(buf, 1, INV_SIZE, f);
    fclose(f);
}

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

static float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void ui_window_to_drawable(SDL_Window* window, int window_x, int window_y, float* drawable_x, float* drawable_y) {
    int ww = 1, wh = 1, dw = 1, dh = 1;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    float scale_x = ww > 0 ? (float)dw / (float)ww : 1.0f;
    float scale_y = wh > 0 ? (float)dh / (float)wh : 1.0f;
    *drawable_x = (float)window_x * scale_x;
    *drawable_y = (float)window_y * scale_y;
}

static int chunk_within_render_distance(float cam_x, float cam_z, float chunk_x, float chunk_z, float render_distance_chunks) {
    float center_x = chunk_x + CHUNK_SIZE_X * 0.5f;
    float center_z = chunk_z + CHUNK_SIZE_Z * 0.5f;
    float max_dist = render_distance_chunks * (float)CHUNK_SIZE_X;
    float dx = center_x - cam_x;
    float dz = center_z - cam_z;
    return dx * dx + dz * dz <= max_dist * max_dist;
}
typedef struct {
    int fps_cap;
    float render_distance_chunks;
    int gravity_enabled;
} Settings;

static void load_settings(const char* path, Settings* settings) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    int fps_cap = settings->fps_cap;
    float render_distance_chunks = settings->render_distance_chunks;
    int gravity_enabled = settings->gravity_enabled;
    if (fscanf(f, "%d %f %d", &fps_cap, &render_distance_chunks, &gravity_enabled) == 3) {
        settings->fps_cap = fps_cap;
        settings->render_distance_chunks = render_distance_chunks;
        settings->gravity_enabled = gravity_enabled ? 1 : 0;
    }
    fclose(f);
}

static void save_settings(const char* path, const Settings* settings) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d %.3f %d\n", settings->fps_cap, settings->render_distance_chunks, settings->gravity_enabled ? 1 : 0);
    fclose(f);
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

static int raycast_block_selection(World* world, vec3 origin, vec3 direction, float max_dist, BlockSelection* sel, int include_water) {
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
        if (b && (block_is_solid(b->type) || (include_water && b->type == BLOCK_WATER))) {
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
        "src/textures/oak_planks.png",
        "src/textures/water.png",
        "src/textures/oaklog_side.png",
        "src/textures/oaklog_top.png",
        "src/textures/oak_leaves.png",
        "src/textures/glass_block.png",
    };
    GLuint texture = load_texture_array(world_textures, 10);

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

    GLuint item_textures[ITEM_COUNT] = {0};
    for (int i = 1; i < ITEM_COUNT; i++) {
        int hw2, hh2, hch2;
        unsigned char* hb = stbi_load(item_defs[i].tex_path, &hw2, &hh2, &hch2, 4);
        if (!hb) continue;
        glGenTextures(1, &item_textures[i]);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, item_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hw2, hh2, 0, GL_RGBA, GL_UNSIGNED_BYTE, hb);
        stbi_image_free(hb);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    int inventory[INV_SIZE] = {0};
    for (int i = 1; i < ITEM_COUNT; i++) inventory[INV_HOTBAR_START + i - 1] = i;
    load_inventory("Savefiles/inventory.bin", inventory);
    for (int i = 1; i < ITEM_COUNT; i++) {
        int present = 0;
        for (int j = 0; j < INV_SIZE; j++) if (inventory[j] == i) present = 1;
        if (!present)
            for (int j = 0; j < INV_SIZE; j++)
                if (inventory[j] == 0) { inventory[j] = i; break; }
    }
    int selected_slot = 0;
    int inventory_open = 0;
    int drag_item = 0, drag_from = -1;

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

    const float WALK_SPEED=4.317f, SPRINT_SPEED=7.5f, CROUCH_SPEED=1.31f;
    const float FLY_SPEED=10.92f, FLY_SPRINT_SPEED=21.6f;
    const float GRAVITY=28.0f, JUMP_IMPULSE=8.9f, FLOOR_Y=-4.0f;
    const float HFRICTION_PS=0.03f, AIR_ACCEL_FRAC=0.2f;
    const float STAND_HEIGHT=1.8f, CROUCH_HEIGHT=1.35f;
    const float STAND_EYE=1.62f, CROUCH_EYE=1.27f;
    const float FOG_R=0.4f, FOG_G=0.7f, FOG_B=1.0f;

    float vel_x=0, vel_y=0, vel_z=0;
    int jump_buffer=0, crouching=0;
    int mouse_right_held=0;
    float place_timer=0.0f;
    int show_fps=0, fps_count=0, fps_display=0;
    float fps_timer=0.0f;
    int fps_cap = 120;
    float render_distance_chunks = 6.0f;
    float reach_distance = 7.5f;
    int paused_drag_slider = 0;
    Uint32 last_time = SDL_GetTicks();
    Uint64 fps_perf_freq = SDL_GetPerformanceFrequency();
    Uint64 fps_frame_start_counter = SDL_GetPerformanceCounter();
    double fps_next_deadline_seconds = (double)fps_frame_start_counter / (double)fps_perf_freq;
    Settings settings = { fps_cap, render_distance_chunks, gravity_enabled };
    load_settings("Savefiles/settings.cfg", &settings);
    fps_cap = settings.fps_cap;
    render_distance_chunks = settings.render_distance_chunks;
    gravity_enabled = settings.gravity_enabled;

    while (running) {
        int break_requested=0, place_requested=0;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_F3)
                show_fps = !show_fps;
            if (event.type == SDL_MOUSEWHEEL && !paused && !inventory_open) {
                selected_slot = (selected_slot - event.wheel.y % 9 + 9) % 9;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && !paused && !inventory_open) {
                SDL_Keycode k = event.key.keysym.sym;
                if (k >= SDLK_1 && k <= SDLK_9) selected_slot = k - SDLK_1;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_e && !paused) {
                inventory_open = !inventory_open;
                if (inventory_open) {
                    mouse_right_held = 0;
                    SDL_SetRelativeMouseMode(SDL_FALSE); SDL_ShowCursor(SDL_ENABLE);
                } else {
                    inventory_put_back(inventory, &drag_item, &drag_from);
                    SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click = 1;
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                if (inventory_open) {
                    inventory_open = 0;
                    inventory_put_back(inventory, &drag_item, &drag_from);
                    SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click = 1;
                } else {
                    paused = !paused;
                    paused_drag_slider = 0;
                    if (paused) { SDL_SetRelativeMouseMode(SDL_FALSE); SDL_ShowCursor(SDL_ENABLE); }
                    else { SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click = 1; }
                }
            } else if (!paused && event.type == SDL_KEYDOWN && event.key.repeat == 0
                       && event.key.keysym.sym == SDLK_SPACE && gravity_enabled) {
                (void)0;
            } else if (paused && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                float menu_w=360.0f, row_h=64.0f, row_gap=84.0f;
                float menu_x=((float)sw-menu_w)*0.5f;
                float slider_y1=(float)sh*0.23f;
                float slider_y2=slider_y1+row_gap;
                float button_y1=slider_y2+row_gap;
                float button_y2=button_y1+row_gap;
                float mouse_x=0.0f, mouse_y=0.0f;
                ui_window_to_drawable(window, event.button.x, event.button.y, &mouse_x, &mouse_y);
                if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, slider_y1, menu_w, row_h)) {
                    paused_drag_slider = 1;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, slider_y2, menu_w, row_h)) {
                    paused_drag_slider = 2;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y1, menu_w, row_h)) {
                    paused=0; SDL_SetRelativeMouseMode(SDL_TRUE); SDL_ShowCursor(SDL_DISABLE); cam.first_click=1;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y2, menu_w, row_h)) {
                    gravity_enabled=!gravity_enabled; if(!gravity_enabled) vel_y=0.0f;
                }
                if (paused_drag_slider == 1 || paused_drag_slider == 2) {
                    float t = clampf_local((mouse_x - menu_x) / menu_w, 0.0f, 1.0f);
                    if (paused_drag_slider == 1) {
                        fps_cap = 30 + (int)(t * 1970.0f + 0.5f);
                        fps_next_deadline_seconds = (double)SDL_GetPerformanceCounter() / (double)fps_perf_freq;
                    } else {
                        render_distance_chunks = 4.0f + t * 28.0f;
                    }
                }
            } else if (paused && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                paused_drag_slider = 0;
            } else if (!paused && inventory_open && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                InvLayout inv_layout; inventory_layout(sw, sh, &inv_layout);
                float mx=0.0f, my=0.0f;
                ui_window_to_drawable(window, event.button.x, event.button.y, &mx, &my);
                int slot = inventory_slot_at(&inv_layout, mx, my);
                if (slot >= 0 && inventory[slot] != 0) {
                    if (SDL_GetModState() & KMOD_SHIFT) {
                        inventory_shift_click(inventory, slot);
                    } else {
                        drag_item = inventory[slot];
                        drag_from = slot;
                        inventory[slot] = 0;
                    }
                }
            } else if (!paused && inventory_open && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                if (drag_item) {
                    int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                    InvLayout inv_layout; inventory_layout(sw, sh, &inv_layout);
                    float mx=0.0f, my=0.0f;
                    ui_window_to_drawable(window, event.button.x, event.button.y, &mx, &my);
                    int slot = inventory_slot_at(&inv_layout, mx, my);
                    if (slot >= 0) {
                        if (inventory[slot] != 0 && drag_from >= 0)
                            inventory[drag_from] = inventory[slot];
                        inventory[slot] = drag_item;
                        drag_item = 0;
                        drag_from = -1;
                    } else {
                        for (int i = 0; i < INV_MAIN_COUNT && drag_item; i++)
                            if (inventory[i] == 0) { inventory[i] = drag_item; drag_item = 0; drag_from = -1; }
                        inventory_put_back(inventory, &drag_item, &drag_from);
                    }
                }
            } else if (!paused && !inventory_open && event.type == SDL_MOUSEBUTTONDOWN) {
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
        world_stream_missing(world, 4);

        {
            int rebuilt = 0;
            for (int i = 0; i < WORLD_SLOTS && rebuilt < 1; i++) {
                WorldSlot* s = &world->slots[i];
                if (s->loaded && (!s->mesh_valid || s->mesh_dirty)) {
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

        float render_distance_effective_chunks = render_distance_chunks;
        float fog_end = fmaxf(render_distance_effective_chunks * (float)CHUNK_SIZE_X * 0.55f, 24.0f);
        float fog_start = fmaxf(fog_end * 0.30f, 8.0f);
        if (fog_start > fog_end - 6.0f) fog_start = fmaxf(fog_end - 6.0f, 0.0f);

        mat4 model; glm_mat4_identity(model);

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_time) * 0.001f;
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;

        if (!paused) world_update_water(world, dt);

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
            static const Uint8 locked_keys[SDL_NUM_SCANCODES];
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            if (inventory_open) keys = locked_keys;
            else camera_rotate(&cam, mouse_dx, mouse_dy);

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

            Block* body_block = world_get_block(world,
                (int)floorf(cam.position[0]),
                (int)floorf(cam.position[1] + 0.4f),
                (int)floorf(cam.position[2]));
            int in_water = body_block && body_block->type == BLOCK_WATER;
            float eye_y_preview = cam.position[1] + (crouching ? CROUCH_EYE : STAND_EYE);
            Block* head_block = world_get_block(world,
                (int)floorf(cam.position[0]),
                (int)floorf(eye_y_preview),
                (int)floorf(cam.position[2]));
            int head_in_water = head_block && head_block->type == BLOCK_WATER;

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
                if (grounded && keys[SDL_SCANCODE_SPACE] && !crouching && vel_y <= 0.0f) { vel_y=JUMP_IMPULSE; grounded=0; }
                if (in_water && keys[SDL_SCANCODE_SPACE]) vel_y = 4.0f;
                if (!grounded) vel_y -= (in_water ? GRAVITY*0.3f : GRAVITY)*dt;
                if (in_water && vel_y < -3.0f) vel_y = -3.0f;
                vec3 next_pos; glm_vec3_copy(cam.position, next_pos);
                next_pos[1] += vel_y*dt;
                if (!player_collides_at_h(world, next_pos, cur_height)) {
                    cam.position[1]=next_pos[1];
                } else {
                    int escaped = 0;
                    if (in_water && keys[SDL_SCANCODE_SPACE]) {
                        for (float lift = 0.05f; lift <= 1.2f; lift += 0.05f) {
                            next_pos[1] = cam.position[1] + lift;
                            if (!player_collides_at_h(world, next_pos, cur_height)) {
                                cam.position[1] = next_pos[1];
                                vel_y = 0.0f;
                                escaped = 1;
                                break;
                            }
                        }
                    }
                    if (!escaped) vel_y=0.0f;
                }
                vec3 move_xz = {vel_x*dt, 0.0f, vel_z*dt};
                if (in_water && !head_in_water && keys[SDL_SCANCODE_SPACE]) {
                    vec3 xz_try; glm_vec3_copy(cam.position, xz_try);
                    xz_try[0] += move_xz[0];
                    xz_try[2] += move_xz[2];
                    if (player_collides_at_h(world, xz_try, cur_height)) {
                        int climbed = 0;
                        for (float lift = 0.25f; lift <= 0.70f; lift += 0.05f) {
                            vec3 climb_try; glm_vec3_copy(xz_try, climb_try);
                            climb_try[1] += lift;
                            Block* climb_body = world_get_block(world,
                                (int)floorf(climb_try[0]),
                                (int)floorf(climb_try[1] + 0.4f),
                                (int)floorf(climb_try[2]));
                            float climb_eye_y = climb_try[1] + (crouching ? CROUCH_EYE : STAND_EYE);
                            Block* climb_head = world_get_block(world,
                                (int)floorf(climb_try[0]),
                                (int)floorf(climb_eye_y),
                                (int)floorf(climb_try[2]));
                            if ((climb_body && climb_body->type == BLOCK_WATER) ||
                                (climb_head && climb_head->type == BLOCK_WATER)) continue;
                            if (!player_collides_at_h(world, climb_try, cur_height)) {
                                glm_vec3_copy(climb_try, cam.position);
                                climbed = 1;
                                break;
                            }
                        }
                        if (!climbed)
                            move_with_collision_h(&cam, world, move_xz, cur_height, 0);
                    } else {
                        move_with_collision_h(&cam, world, move_xz, cur_height, 0);
                    }
                } else if (in_water) {
                    move_with_collision_h(&cam, world, move_xz, cur_height, 0);
                } else {
                    move_with_collision_h(&cam, world, move_xz, cur_height, crouching);
                }
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
            raycast_block_selection(world, cam.position, cam.orientation, reach_distance, &sel, 0);
            const ItemDef* held_item = &item_defs[inventory[INV_HOTBAR_START + selected_slot]];

            if (break_requested && sel.hit) {
                if (world_set_block(world, sel.block_x, sel.block_y, sel.block_z, BLOCK_AIR)) {
                    sel.hit=0; raycast_block_selection(world, cam.position, cam.orientation, reach_distance, &sel, 0);
                }
            }
            if (place_requested && held_item->is_bucket) {
                BlockSelection wsel = {0};
                raycast_block_selection(world, cam.position, cam.orientation, reach_distance, &wsel, 1);
                if (wsel.hit) {
                    Block* tb = world_get_block(world, wsel.block_x, wsel.block_y, wsel.block_z);
                    if (tb && tb->type == BLOCK_WATER) {
                        if (tb->level == WATER_LEVEL_SOURCE)
                            world_set_block(world, wsel.block_x, wsel.block_y, wsel.block_z, BLOCK_AIR);
                        else
                            world_place_water(world, wsel.block_x, wsel.block_y, wsel.block_z);
                    } else {
                        Block* pb = world_get_block(world, wsel.place_x, wsel.place_y, wsel.place_z);
                        if (pb && !block_is_solid(pb->type))
                            world_place_water(world, wsel.place_x, wsel.place_y, wsel.place_z);
                    }
                }
            } else if (place_requested && sel.hit) {
                Block* pb = world_get_block(world, sel.place_x, sel.place_y, sel.place_z);
                if (pb && !block_is_solid(pb->type) && held_item->block != BLOCK_AIR) {
                    BlockType old_type = pb->type;
                    uint8_t old_level = pb->level;
                    pb->type = held_item->block;
                    vec3 foot = {cam.position[0], real_y, cam.position[2]};
                    if (!player_collides_at_h(world, foot, cur_height)) {
                        pb->type = old_type; pb->level = old_level;
                        world_set_block(world, sel.place_x, sel.place_y, sel.place_z, held_item->block);
                        sel.hit=0; raycast_block_selection(world,cam.position,cam.orientation,reach_distance,&sel,0);
                    } else { pb->type = old_type; pb->level = old_level; }
                }
            }

            camera_update(&cam, 45.0f, 0.1f, 1000.0f);
            cam.position[1] = real_y;

            glUniformMatrix4fv(u_camMatrix, 1, GL_FALSE, (float*)cam.camera_matrix);
            glUniformMatrix4fv(u_view,      1, GL_FALSE, (float*)cam.view);
            glUniformMatrix4fv(u_proj,      1, GL_FALSE, (float*)cam.proj);
            if (head_in_water)
                glUniform3f(u_fog_color, FOG_R * 0.78f, FOG_G * 0.86f, fminf(FOG_B * 1.12f, 1.0f));
            else
                glUniform3f(u_fog_color, FOG_R, FOG_G, FOG_B);
            glUniform1f(u_fog_start, fog_start);
            glUniform1f(u_fog_end,   fog_end);
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
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if (!chunk_in_frustum(frustum, wx, wz)) continue;
                mat4 chunk_model; glm_mat4_identity(chunk_model);
                vec3 chunk_offset = {wx, 0.0f, wz};
                glm_translate(chunk_model, chunk_offset);
                glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)chunk_model);
                mesh_draw(&s->mesh);
            }

            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            for (int i = 0; i < WORLD_SLOTS; i++) {
                WorldSlot* s = &world->slots[i];
                if (!s->loaded || !s->mesh_valid) continue;
                if (s->water_mesh.index_count == 0) continue;
                float wx = (float)(s->chunk.cx * CHUNK_SIZE_X);
                float wz = (float)(s->chunk.cz * CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if (!chunk_in_frustum(frustum, wx, wz)) continue;
                mat4 chunk_model; glm_mat4_identity(chunk_model);
                vec3 chunk_offset = {wx, 0.0f, wz};
                glm_translate(chunk_model, chunk_offset);
                glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)chunk_model);
                mesh_draw(&s->water_mesh);
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);

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

            if (head_in_water) {
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
                float tint[12]; int tintc = 0;
                append_rect(tint, &tintc, 0.0f, 0.0f, (float)screen_w, (float)screen_h);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tintc, tint);
                glUniform2f(u_ui_screenSize, (float)screen_w, (float)screen_h);
                glUniform4f(u_ui_color, 0.08f, 0.24f, 0.42f, 0.24f);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
                glUseProgram(shaderProgram);
                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
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

            {
                glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                float slot_size = (float)screen_h * 0.07f;
                if (slot_size < 20.0f) slot_size = 20.0f;
                if (slot_size > 64.0f) slot_size = 64.0f;
                float pad = slot_size * 0.1f;
                float total_w = 9.0f * slot_size + 8.0f * pad;
                float hx = ((float)screen_w - total_w) * 0.5f;
                float hy = (float)screen_h - slot_size - slot_size * 0.15f;

                glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
                glUniform2f(u_ui_screenSize, (float)screen_w, (float)screen_h);
                float slot_verts[12]; int svc;
                for (int si = 0; si < 9; si++) {
                    float sx2 = hx + si * (slot_size + pad);
                    svc = 0; append_rect(slot_verts, &svc, sx2, hy, slot_size, slot_size);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*svc, slot_verts);
                    glUniform4f(u_ui_color, 0.15f, 0.15f, 0.15f, 0.7f);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    if (si == selected_slot) {
                        float brd = slot_size * 0.06f;
                        float bverts[48]; int bvc = 0;
                        append_rect(bverts, &bvc, sx2-brd, hy-brd, slot_size+2*brd, brd);
                        append_rect(bverts, &bvc, sx2-brd, hy+slot_size, slot_size+2*brd, brd);
                        append_rect(bverts, &bvc, sx2-brd, hy, brd, slot_size);
                        append_rect(bverts, &bvc, sx2+slot_size, hy, brd, slot_size);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*bvc, bverts);
                        glUniform4f(u_ui_color, 1.0f, 1.0f, 1.0f, 0.9f);
                        glDrawArrays(GL_TRIANGLES, 0, bvc/2);
                    }
                }
                glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

                for (int si = 0; si < 9; si++) {
                    GLuint itex = item_textures[inventory[INV_HOTBAR_START + si]];
                    if (!itex) continue;
                    float sx2 = hx + si * (slot_size + pad);
                    float inner = slot_size * 0.75f;
                    float ox = sx2 + (slot_size - inner) * 0.5f;
                    float oy = hy + (slot_size - inner) * 0.5f;
                    float iv[24] = {
                        ox,       oy,       0,0,
                        ox+inner, oy,       1,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy,       0,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy+inner, 0,1,
                    };
                    glUseProgram(buttonProgram); glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glUniform2f(u_btn_screenSize, (float)screen_w, (float)screen_h);
                    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, itex);
                    glUniform1i(u_btn_tex, 3);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
                }
                glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
            }

            if (inventory_open) {
                glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                InvLayout inv_layout; inventory_layout(screen_w, screen_h, &inv_layout);
                float panel_pad = inv_layout.cell * 0.35f;
                float grid_w = 9.0f * inv_layout.cell + 8.0f * inv_layout.pad;
                float grid_h = 4.0f * inv_layout.cell + 3.0f * inv_layout.pad + inv_layout.gap;

                glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
                glUniform2f(u_ui_screenSize, (float)screen_w, (float)screen_h);
                float rect[12]; int rc = 0;
                append_rect(rect, &rc, 0.0f, 0.0f, (float)screen_w, (float)screen_h);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*rc, rect);
                glUniform4f(u_ui_color, 0.0f, 0.0f, 0.0f, 0.45f);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                rc = 0;
                append_rect(rect, &rc, inv_layout.x - panel_pad, inv_layout.y - panel_pad,
                            grid_w + 2.0f*panel_pad, grid_h + 2.0f*panel_pad);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*rc, rect);
                glUniform4f(u_ui_color, 0.12f, 0.12f, 0.12f, 0.92f);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                glUniform4f(u_ui_color, 0.24f, 0.24f, 0.24f, 0.95f);
                for (int i = 0; i < INV_SIZE; i++) {
                    float rx, ry;
                    inventory_slot_rect(&inv_layout, i, &rx, &ry);
                    rc = 0;
                    append_rect(rect, &rc, rx, ry, inv_layout.cell, inv_layout.cell);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*rc, rect);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

                glUseProgram(buttonProgram); glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
                glUniform2f(u_btn_screenSize, (float)screen_w, (float)screen_h);
                glUniform1i(u_btn_tex, 3);
                glActiveTexture(GL_TEXTURE3);
                float inner = inv_layout.cell * 0.75f;
                for (int i = 0; i < INV_SIZE; i++) {
                    GLuint itex = item_textures[inventory[i]];
                    if (!itex) continue;
                    float rx, ry;
                    inventory_slot_rect(&inv_layout, i, &rx, &ry);
                    float ox = rx + (inv_layout.cell - inner) * 0.5f;
                    float oy = ry + (inv_layout.cell - inner) * 0.5f;
                    float iv[24] = {
                        ox,       oy,       0,0,
                        ox+inner, oy,       1,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy,       0,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy+inner, 0,1,
                    };
                    glBindTexture(GL_TEXTURE_2D, itex);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                if (drag_item && item_textures[drag_item]) {
                    int mwx = 0, mwy = 0;
                    SDL_GetMouseState(&mwx, &mwy);
                    float mx = 0.0f, my = 0.0f;
                    ui_window_to_drawable(window, mwx, mwy, &mx, &my);
                    float ox = mx - inner * 0.5f;
                    float oy = my - inner * 0.5f;
                    float iv[24] = {
                        ox,       oy,       0,0,
                        ox+inner, oy,       1,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy,       0,0,
                        ox+inner, oy+inner, 1,1,
                        ox,       oy+inner, 0,1,
                    };
                    glBindTexture(GL_TEXTURE_2D, item_textures[drag_item]);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                glBindTexture(GL_TEXTURE_2D, 0);
                glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
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
            glUniform1f(u_fog_start,fog_start); glUniform1f(u_fog_end,fog_end);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
            glUniform1i(u_tex0,0);

            FPlane frustum[6]; extract_frustum(cam.camera_matrix, frustum);
            for (int i=0;i<WORLD_SLOTS;i++) {
                WorldSlot* s=&world->slots[i];
                if(!s->loaded||!s->mesh_valid) continue;
                float wx=(float)(s->chunk.cx*CHUNK_SIZE_X), wz=(float)(s->chunk.cz*CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if(!chunk_in_frustum(frustum,wx,wz)) continue;
                mat4 cm; glm_mat4_identity(cm);
                vec3 co={wx,0.0f,wz}; glm_translate(cm,co);
                glUniformMatrix4fv(u_model,1,GL_FALSE,(float*)cm);
                mesh_draw(&s->mesh);
            }

            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE);
            for (int i=0;i<WORLD_SLOTS;i++) {
                WorldSlot* s=&world->slots[i];
                if(!s->loaded||!s->mesh_valid) continue;
                if (s->water_mesh.index_count == 0) continue;
                float wx=(float)(s->chunk.cx*CHUNK_SIZE_X), wz=(float)(s->chunk.cz*CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if(!chunk_in_frustum(frustum,wx,wz)) continue;
                mat4 cm; glm_mat4_identity(cm);
                vec3 co={wx,0.0f,wz}; glm_translate(cm,co);
                glUniformMatrix4fv(u_model,1,GL_FALSE,(float*)cm);
                mesh_draw(&s->water_mesh);
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);

            glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
            float overlay[12]; int oc=0;
            append_rect(overlay,&oc,0,0,(float)screen_w,(float)screen_h);
            glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(float)*oc,overlay);
            glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
            glUniform4f(u_ui_color,0.35f,0.35f,0.35f,0.65f);
            glDrawArrays(GL_TRIANGLES,0,6);

            float panel_w=360.0f, row_h=64.0f, row_gap=84.0f, slider_h=12.0f, knob_w=14.0f;
            float panel_x=((float)screen_w-panel_w)*0.5f;
            float slider_y1=(float)screen_h*0.23f;
            float slider_y2=slider_y1+row_gap;
            float button_y1=slider_y2+row_gap;
            float button_y2=button_y1+row_gap;

            int mouse_x=0, mouse_y=0;
            Uint32 mouse_mask = SDL_GetMouseState(&mouse_x, &mouse_y);
            if (paused && paused_drag_slider != 0 && (mouse_mask & SDL_BUTTON(SDL_BUTTON_LEFT))) {
                float mx=0.0f, my=0.0f;
                ui_window_to_drawable(window, mouse_x, mouse_y, &mx, &my);
                float t = clampf_local((mx - panel_x) / panel_w, 0.0f, 1.0f);
                    if (paused_drag_slider == 1) {
                        fps_cap = 30 + (int)(t * 1970.0f + 0.5f);
                        fps_next_deadline_seconds = (double)SDL_GetPerformanceCounter() / (double)fps_perf_freq;
                    }
                if (paused_drag_slider == 2) render_distance_chunks = 4.0f + t * 28.0f;
            }
    float fog_end = fminf(24.0f + render_distance_effective_chunks * 2.5f, 144.0f);
    float fog_start = fmaxf(fog_end * 0.40f, 12.0f);
    if (fog_start > fog_end - 6.0f) fog_start = fmaxf(fog_end - 6.0f, 0.0f);

            glUseProgram(buttonProgram); glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER,buttonVBO);
            glUniform2f(u_btn_screenSize,(float)screen_w,(float)screen_h);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,buttonTexture);
            glUniform1i(u_btn_tex,1);
            float boffsets[]={0,84};
            for(int b=0;b<2;b++){
                float by2=button_y1+boffsets[b];
                float bv[24]={panel_x,by2,0,0, panel_x+panel_w,by2,1,0, panel_x+panel_w,by2+row_h,1,1, panel_x,by2,0,0, panel_x+panel_w,by2+row_h,1,1, panel_x,by2+row_h,0,1};
                glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(bv),bv);
                glDrawArrays(GL_TRIANGLES,0,6);
            }

            glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
            float fps_track = clampf_local((float)(fps_cap - 30) / 1970.0f, 0.0f, 1.0f);
            float rd_track = clampf_local((render_distance_chunks - 4.0f) / 28.0f, 0.0f, 1.0f);

            float track_w = panel_w;
            float track_h = slider_h;
            float knob_h = 28.0f;
            float fps_knob_x = panel_x + fps_track * track_w - knob_w * 0.5f;
            float rd_knob_x  = panel_x + rd_track * track_w - knob_w * 0.5f;

            float fps_track_rect[12] = {panel_x, slider_y1, panel_x + track_w, slider_y1, panel_x + track_w, slider_y1 + track_h, panel_x, slider_y1, panel_x + track_w, slider_y1 + track_h, panel_x, slider_y1 + track_h};
            float rd_track_rect[12]  = {panel_x, slider_y2, panel_x + track_w, slider_y2, panel_x + track_w, slider_y2 + track_h, panel_x, slider_y2, panel_x + track_w, slider_y2 + track_h, panel_x, slider_y2 + track_h};
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fps_track_rect), fps_track_rect);
            glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
            glUniform4f(u_ui_color,0.18f,0.18f,0.18f,0.85f);
            glDrawArrays(GL_TRIANGLES,0,6);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rd_track_rect), rd_track_rect);
            glDrawArrays(GL_TRIANGLES,0,6);

            float fps_knob[12] = {fps_knob_x, slider_y1 - 8.0f, fps_knob_x + knob_w, slider_y1 - 8.0f, fps_knob_x + knob_w, slider_y1 + knob_h, fps_knob_x, slider_y1 - 8.0f, fps_knob_x + knob_w, slider_y1 + knob_h, fps_knob_x, slider_y1 + knob_h};
            float rd_knob[12]  = {rd_knob_x,  slider_y2 - 8.0f, rd_knob_x + knob_w,  slider_y2 - 8.0f, rd_knob_x + knob_w,  slider_y2 + knob_h, rd_knob_x,  slider_y2 - 8.0f, rd_knob_x + knob_w,  slider_y2 + knob_h, rd_knob_x,  slider_y2 + knob_h};
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fps_knob), fps_knob);
            glUniform4f(u_ui_color,0.90f,0.90f,0.90f,1.0f);
            glDrawArrays(GL_TRIANGLES,0,6);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rd_knob), rd_knob);
            glDrawArrays(GL_TRIANGLES,0,6);

            char fps_label[32];
            char rd_label[32];
            snprintf(fps_label, sizeof(fps_label), "fps cap: %d", fps_cap);
            snprintf(rd_label, sizeof(rd_label), "render distance: %.0f chunks", render_distance_effective_chunks);
            float label_verts[8192]; int label_count=0;
            const char* pause_title = "pause menu";
            float title_w = 0.0f;
            for (const char* ch = pause_title; *ch; ++ch) title_w += (*ch == ' ') ? 4.0f * 2.6f : 6.0f * 2.6f;
            build_text_vertices(pause_title, panel_x + (panel_w - title_w) * 0.5f, (float)screen_h*0.10f, 2.6f, label_verts, &label_count);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*label_count, label_verts);
            glUniform4f(u_ui_color,1.0f,1.0f,1.0f,1.0f);
            glDrawArrays(GL_TRIANGLES,0,label_count/2);

            label_count = 0;
            build_text_vertices(fps_label, panel_x, slider_y1 - 28.0f, 2.0f, label_verts, &label_count);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*label_count, label_verts);
            glDrawArrays(GL_TRIANGLES,0,label_count/2);

            label_count = 0;
            build_text_vertices(rd_label, panel_x, slider_y2 - 28.0f, 2.0f, label_verts, &label_count);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*label_count, label_verts);
            glDrawArrays(GL_TRIANGLES,0,label_count/2);

            glUseProgram(uiProgram); glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER,uiVBO);
            float tverts[32768]; int tc=0; float ts=2.2f;
            const char* lbls[]={ "continue playing",
                                  gravity_enabled?"gravity on":"gravity off" };
            float ypos[]={button_y1,button_y2};
            for(int b=0;b<2;b++){
                tc=0; float tw=0;
                for(const char* ch=lbls[b];*ch;++ch) tw+=(*ch==' ')?4.0f*ts:6.0f*ts;
                build_text_vertices(lbls[b],panel_x+(panel_w-tw)*0.5f,ypos[b]+(row_h-7*ts)*0.5f,ts,tverts,&tc);
                glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(float)*tc,tverts);
                glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
                glUniform4f(u_ui_color,0.15f,0.15f,0.15f,1);
                glDrawArrays(GL_TRIANGLES,0,tc/2);
            }
            glBindBuffer(GL_ARRAY_BUFFER,0); glBindVertexArray(0);
            glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
        }

        if (fps_cap > 0) {
            while (1) {
                Uint64 frame_now_counter = SDL_GetPerformanceCounter();
                double now_seconds = (double)frame_now_counter / (double)fps_perf_freq;
                if (fps_next_deadline_seconds < now_seconds) fps_next_deadline_seconds = now_seconds;
                double remaining_seconds = fps_next_deadline_seconds - now_seconds;
                if (remaining_seconds <= 0.0) break;
                if (remaining_seconds > 0.002) {
                    SDL_Delay((Uint32)((remaining_seconds - 0.001) * 1000.0));
                }
            }
        }

        SDL_GL_SwapWindow(window);
        fps_next_deadline_seconds += 1.0 / (double)fps_cap;
        fps_frame_start_counter = SDL_GetPerformanceCounter();
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

    settings.fps_cap = fps_cap;
    settings.render_distance_chunks = render_distance_chunks;
    settings.gravity_enabled = gravity_enabled;
    save_settings("Savefiles/settings.cfg", &settings);
    inventory_put_back(inventory, &drag_item, &drag_from);
    save_inventory("Savefiles/inventory.bin", inventory);

    world_save_all_dirty(world);
    world_free(world);
    glDeleteTextures(1,&texture); glDeleteTextures(1,&buttonTexture); glDeleteTextures(1,&crosshairTexture);
    for (int i = 0; i < ITEM_COUNT; i++) if (item_textures[i]) glDeleteTextures(1, &item_textures[i]);
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