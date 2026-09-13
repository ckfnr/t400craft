//   /$$   /$$   /$$  /$$$$$$   /$$$$$$                                /$$$$$$   /$$  
//  | $$  | $$  | $$ /$$$_  $$ /$$$_  $$                              /$$__  $$ | $$    
// /$$$$$$| $$  | $$| $$$$\ $$| $$$$\ $$  /$$$$$$$  /$$$$$$  /$$$$$$ | $$  \__//$$$$$$  
//|_  $$_/| $$$$$$$$| $$ $$ $$| $$ $$ $$ /$$_____/ /$$__  $$|____  $$| $$$$   |_  $$_/  
//  | $$  |_____  $$| $$\ $$$$| $$\ $$$$| $$      | $$  \__/ /$$$$$$$| $$_/     | $$    
//  | $$ /$$    | $$| $$ \ $$$| $$ \ $$$| $$      | $$      /$$__  $$| $$       | $$ /$$
//  |  $$$$/    | $$|  $$$$$$/|  $$$$$$/|  $$$$$$$| $$     |  $$$$$$$| $$       |  $$$$/
//   \___/      |__/ \______/  \______/  \_______/|__/      \_______/|__/        \___/  
//made by Cesario Kufner



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include "Shaders/shader_loader.h"
#include "Mesh/mesh.h"
#include "include/stb_image.h"
#include "include/cglm/include/cglm/cglm.h"
#include "Camera/Camera.h"
#include "World/block.h"
#include "World/items.h"
#include "World/chunk.h"
#include "World/world.h"
#include "World/chunk_mesh.h"
#include "World/menu.h"

const unsigned int width = 800;
const unsigned int height = 800;

/* Future survival-mode switch: creative block breaking must not drop items. */
#define SURVIVAL_MODE 0

static int g_wayland_video_driver = 0;
static int g_use_relative_mouse = 1;

static void capture_relative_mouse(SDL_Window* window) {
    if (window) {
        int want_grab = g_use_relative_mouse ? (g_wayland_video_driver ? SDL_FALSE : SDL_TRUE) : SDL_TRUE;
        SDL_SetWindowGrab(window, want_grab ? SDL_TRUE : SDL_FALSE);
    }
    if (g_use_relative_mouse) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_MOUSEMOTION);
    if (g_use_relative_mouse) {
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

static void release_relative_mouse(SDL_Window* window) {
    if (window) {
        SDL_SetWindowGrab(window, SDL_FALSE);
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_FlushEvent(SDL_MOUSEMOTION);
}

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
    static const unsigned char j_glyph[7] = {2,  0,   2,   2,   2,   18,  12 };
    static const unsigned char k_glyph[7] = {17, 18,  20,  28,  20,  18,  17 };
    static const unsigned char l_glyph[7] = {16, 16,  16,  16,  16,  16,  31 };
    static const unsigned char m_glyph[7] = {17, 27,  21,  21,  17,  17,  17 };
    static const unsigned char n_glyph[7] = {17, 25,  25,  21,  19,  19,  17 };
    static const unsigned char o_glyph[7] = {14, 17,  17,  17,  17,  17,  14 };
    static const unsigned char p_glyph[7] = {30, 17,  17,  30,  16,  16,  16 };
    static const unsigned char q_glyph[7] = {14, 17,  17,  17,  21,  18,  13 };
    static const unsigned char r_glyph[7] = {30, 17,  17,  30,  20,  18,  17 };
    static const unsigned char s_glyph[7] = {14, 17,  16,  14,  1,   17,  14 };
    static const unsigned char t_glyph[7] = {31, 4,   4,   4,   4,   4,   4  };
    static const unsigned char u_glyph[7] = {17, 17,  17,  17,  17,  17,  14 };
    static const unsigned char v_glyph[7] = {17, 17,  17,  17,  10,  10,  4  };
    static const unsigned char w_glyph[7] = {17, 17,  17,  21,  21,  27,  17 };
    static const unsigned char x_glyph[7] = {17, 17,  10,  4,   10,  17,  17 };
    static const unsigned char y_glyph[7] = {17, 17,  10,  4,   4,   4,   4  };
    static const unsigned char z_glyph[7] = {31, 1,   2,   4,   8,   16,  31 };
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
    static const unsigned char hyphen_glyph[7] = {0, 0, 0, 31, 0, 0, 0};
    static const unsigned char colon_glyph[7]  = {0, 4, 0, 0, 0, 4, 0};
    static const unsigned char amp_glyph[7]    = {12, 18, 20, 8, 21, 18, 13};
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    switch (c) {
        case 'a': return a_glyph; case 'b': return b_glyph; case 'c': return c_glyph;
        case 'd': return d_glyph; case 'e': return e_glyph; case 'f': return f_glyph;
        case 'g': return g_glyph; case 'h': return h_glyph; case 'i': return i_glyph;
        case 'j': return j_glyph; case 'k': return k_glyph; case 'l': return l_glyph;
        case 'm': return m_glyph;
        case 'n': return n_glyph; case 'o': return o_glyph; case 'p': return p_glyph;
        case 'q': return q_glyph; case 'r': return r_glyph; case 's': return s_glyph;
        case 't': return t_glyph;
        case 'u': return u_glyph; case 'v': return v_glyph; case 'w': return w_glyph;
        case 'x': return x_glyph; case 'y': return y_glyph; case 'z': return z_glyph;
        case '0': return zero_glyph;  case '1': return one_glyph;
        case '2': return two_glyph;   case '3': return three_glyph;
        case '4': return four_glyph;  case '5': return five_glyph;
        case '6': return six_glyph;   case '7': return seven_glyph;
        case '8': return eight_glyph; case '9': return nine_glyph;
        case '-': return hyphen_glyph; case ':': return colon_glyph;
        case '&': return amp_glyph;
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

static void draw_stack_count(GLuint ui_program, GLuint ui_vao, GLuint ui_vbo,
                             GLint screen_uniform, GLint color_uniform,
                             int screen_w, int screen_h,
                             float x, float y, float size, int count) {
    if (count <= 0) return;
    char text[12];
    snprintf(text, sizeof(text), "%d", count);
    float scale = size * 0.16f;
    if (scale < 1.5f) scale = 1.5f;
    if (scale > 3.0f) scale = 3.0f;
    float vertices[512];
    int vertex_count = 0;
    float text_width = 0.0f;
    for (const char* c = text; *c; c++) text_width += (*c == ' ') ? 4.0f * scale : 6.0f * scale;
    float text_x = x + size - text_width - 3.0f;
    float text_y = y + size - 7.0f * scale - 2.0f;

    glUseProgram(ui_program);
    glBindVertexArray(ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
    build_text_vertices(text, text_x + 1.5f, text_y + 1.5f, scale, vertices, &vertex_count);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * vertex_count, vertices);
    glUniform2f(screen_uniform, (float)screen_w, (float)screen_h);
    glUniform4f(color_uniform, 0.0f, 0.0f, 0.0f, 0.9f);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count / 2);
    vertex_count = 0;
    build_text_vertices(text, text_x, text_y, scale, vertices, &vertex_count);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * vertex_count, vertices);
    glUniform4f(color_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count / 2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static int point_in_rect(int px, int py, float x, float y, float rw, float rh) {
    return px >= (int)x && px <= (int)(x + rw) && py >= (int)y && py <= (int)(y + rh);
}

static int block_is_solid(BlockType type) { return block_opaque(type); }

#define INV_SIZE 36
#define INV_MAIN_COUNT 27
#define INV_HOTBAR_START 27

typedef struct {
    float x, y, cell, pad, gap;
    float craft_x, craft_y, output_x, output_y;
    float palette_x, palette_y;
    int palette_visible_rows;
} InvLayout;

static int palette_scroll = 0;

static void inventory_layout(int screen_w, int screen_h, InvLayout* L) {
    float cell = (float)screen_h * 0.055f;
    if (cell < 30.0f) cell = 30.0f;
    if (cell > 52.0f) cell = 52.0f;
    L->cell = cell;
    L->pad = cell * 0.1f;
    L->gap = cell * 0.6f;
    float grid_w = 9.0f * cell + 8.0f * L->pad;
    float grid_h = 4.0f * cell + 3.0f * L->pad + L->gap;
    float step = cell + L->pad;
    float craft_h = 2.0f * cell + 1.0f * L->pad;
    float total_h = craft_h + cell * 0.8f + grid_h;
    L->x = ((float)screen_w - grid_w) * 0.5f;
    L->y = ((float)screen_h - total_h) * 0.5f + craft_h + cell * 0.8f;
    L->craft_x = L->x;
    L->craft_y = L->y - cell * 0.8f - craft_h;
    L->output_x = L->craft_x + 2.0f * step + 1.0f * step;
    L->output_y = L->craft_y + 0.5f * step;
    L->palette_x = L->craft_x - 2.0f * step - 2.5f * step;
    L->palette_y = L->craft_y;
    float hotbar_bottom = L->y + 3.0f * step + L->gap + cell;
    L->palette_visible_rows = (int)floorf((hotbar_bottom - L->palette_y) / step);
    if (L->palette_visible_rows < 1) L->palette_visible_rows = 1;
    int palette_rows = (ITEM_COUNT - 2) / 2 + 1;
    int max_scroll = palette_rows - L->palette_visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (palette_scroll > max_scroll) palette_scroll = max_scroll;
    if (palette_scroll < 0) palette_scroll = 0;
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

static int inventory_add(int* inv, int* counts, int item, int amount);

static void crafting_slot_rect(const InvLayout* L, int slot, float* rx, float* ry) {
    *rx = L->craft_x + (float)(slot % CRAFTING_GRID_WIDTH) * (L->cell + L->pad);
    *ry = L->craft_y + (float)(slot / CRAFTING_GRID_WIDTH) * (L->cell + L->pad);
}

static int crafting_slot_at(const InvLayout* L, float mx, float my) {
    for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
        float rx, ry;
        crafting_slot_rect(L, i, &rx, &ry);
        if (mx >= rx && mx <= rx + L->cell && my >= ry && my <= ry + L->cell) return i;
    }
    return -1;
}

static int crafting_result_at(const InvLayout* L, float mx, float my) {
    return mx >= L->output_x && mx <= L->output_x + L->cell &&
           my >= L->output_y && my <= L->output_y + L->cell;
}

static void craft_one(int* crafting_items, int* crafting_counts,
                      int* inventory, int* inventory_counts) {
    int recipe_index = item_find_recipe(crafting_items, crafting_counts, CRAFTING_GRID_SIZE);
    if (recipe_index < 0) return;
    const Recipe* recipe = &item_recipes[recipe_index];
    if (inventory_add(inventory, inventory_counts, recipe->result_item, recipe->result_count) != 0)
        return;
    for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
        if (crafting_items[i] == 0) continue;
        int consume = 0;
        for (int j = 0; j < CRAFTING_GRID_SIZE; j++)
            if (recipe->ingredients[j].item == crafting_items[i]) consume += recipe->ingredients[j].count;
        crafting_counts[i] -= consume;
        if (crafting_counts[i] <= 0) {
            crafting_items[i] = 0;
            crafting_counts[i] = 0;
        }
    }
}

static void creative_palette_rect(const InvLayout* L, int item, float* rx, float* ry) {
    int column = (item - 1) % 2;
    int row = (item - 1) / 2 - palette_scroll;
    *rx = L->palette_x + column * (L->cell + L->pad);
    *ry = L->palette_y + row * (L->cell + L->pad);
}

static int creative_palette_at(const InvLayout* L, float mx, float my) {
    for (int item = 1; item < ITEM_COUNT; item++) {
        float rx, ry;
        creative_palette_rect(L, item, &rx, &ry);
        float hotbar_bottom = L->y + 3.0f * (L->cell + L->pad) + L->gap + L->cell;
        if (ry >= L->palette_y && ry + L->cell <= hotbar_bottom &&
            mx >= rx && mx <= rx + L->cell && my >= ry && my <= ry + L->cell) return item;
    }
    return 0;
}

static void inventory_shift_click(int* inv, int* counts, int slot) {
    if (inv[slot] == 0) return;
    int start = slot < INV_MAIN_COUNT ? INV_HOTBAR_START : 0;
    int end = slot < INV_MAIN_COUNT ? INV_SIZE : INV_MAIN_COUNT;
    for (int i = start; i < end; i++) {
        if (inv[i] == 0) {
            inv[i] = inv[slot]; counts[i] = counts[slot];
            inv[slot] = 0; counts[slot] = 0;
            return;
        }
    }
}

static void inventory_put_back(int* inv, int* counts, int* drag_item, int* drag_count, int* drag_from) {
    if (*drag_item) {
        if (*drag_from >= 0 && inv[*drag_from] == 0) {
            inv[*drag_from] = *drag_item; counts[*drag_from] = *drag_count;
        } else {
            for (int i = 0; i < INV_SIZE; i++)
                if (inv[i] == 0) { inv[i] = *drag_item; counts[i] = *drag_count; break; }
        }
    }
    *drag_item = 0; *drag_count = 0; *drag_from = -1;
}

static void load_inventory(const char* path, int* inv, int* counts) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    uint8_t buf[INV_SIZE * 2];
    size_t got = fread(buf, 1, sizeof(buf), f);
    if (got == sizeof(buf)) {
        for (int i = 0; i < INV_SIZE; i++) {
            inv[i] = buf[i] < ITEM_COUNT ? buf[i] : 0;
            counts[i] = inv[i] ? (buf[INV_SIZE + i] ? buf[INV_SIZE + i] : 1) : 0;
        }
    } else if (got == INV_SIZE) {
        for (int i = 0; i < INV_SIZE; i++) {
            inv[i] = buf[i] < ITEM_COUNT ? buf[i] : 0;
            counts[i] = inv[i] ? item_stack_limit(inv[i]) : 0;
        }
    }
    fclose(f);
}

static int save_inventory(const char* path, const int* inv, const int* counts) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t buf[INV_SIZE * 2];
    for (int i = 0; i < INV_SIZE; i++) {
        buf[i] = (uint8_t)inv[i]; buf[INV_SIZE + i] = (uint8_t)counts[i];
    }
    size_t written = fwrite(buf, 1, sizeof(buf), f);
    int closed = fclose(f) == 0;
    return written == sizeof(buf) && closed;
}

static int inventory_add(int* inv, int* counts, int item, int amount) {
    if (item <= 0 || amount <= 0) return amount;
    int limit = item_stack_limit(item);
    for (int i = 0; i < INV_SIZE && amount > 0; i++)
        if (inv[i] == item && counts[i] < limit) {
            int added = amount < limit - counts[i] ? amount : limit - counts[i];
            counts[i] += added; amount -= added;
        }
    for (int i = 0; i < INV_SIZE && amount > 0; i++)
        if (inv[i] == 0) {
            int added = amount < limit ? amount : limit;
            inv[i] = item; counts[i] = added; amount -= added;
        }
    return amount;
}

static int inventory_add_pickup(int* inv, int* counts, int item, int amount) {
    if (item <= 0 || amount <= 0) return amount;
    int limit = item_stack_limit(item);
    for (int i = 0; i < INV_SIZE && amount > 0; i++) {
        if (inv[i] != item || counts[i] >= limit) continue;
        int moved = amount < limit - counts[i] ? amount : limit - counts[i];
        counts[i] += moved;
        amount -= moved;
    }
    for (int i = INV_HOTBAR_START; i < INV_SIZE && amount > 0; i++) {
        if (inv[i] != 0) continue;
        int moved = amount < limit ? amount : limit;
        inv[i] = item;
        counts[i] = moved;
        amount -= moved;
    }
    for (int i = 0; i < INV_MAIN_COUNT && amount > 0; i++) {
        if (inv[i] != 0) continue;
        int moved = amount < limit ? amount : limit;
        inv[i] = item;
        counts[i] = moved;
        amount -= moved;
    }
    return amount;
}

static void return_crafting_to_inventory(int* crafting_items, int* crafting_counts,
                                         int* inv, int* counts) {
    for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
        if (crafting_items[i] == 0) continue;
        int remaining = inventory_add(inv, counts, crafting_items[i], crafting_counts[i]);
        if (remaining == 0) {
            crafting_items[i] = 0;
            crafting_counts[i] = 0;
        } else {
            crafting_counts[i] = remaining;
        }
    }
}

static void drop_inventory_amount(World* world, Camera* cam, int* inv, int* counts,
                                  int slot, int amount) {
    if (slot < 0 || slot >= INV_SIZE || inv[slot] == 0 || counts[slot] <= 0) return;
    if (amount > counts[slot]) amount = counts[slot];
    world_drop_item(world, inv[slot], amount,
                    cam->position[0], cam->position[1] + 0.8f, cam->position[2],
                    cam->orientation[0], cam->orientation[2]);
    counts[slot] -= amount;
    if (counts[slot] <= 0) inv[slot] = 0;
}

static void finish_drag(World* world, Camera* cam, int* inv, int* counts,
                        int* crafting_items, int* crafting_counts,
                        int* drag_item, int* drag_count, int* drag_from) {
    if (!*drag_item || *drag_count <= 0) {
        *drag_item = 0;
        *drag_count = 0;
        *drag_from = -1;
        return;
    }
    if (*drag_from >= 0 && inv[*drag_from] == 0) {
        inv[*drag_from] = *drag_item;
        counts[*drag_from] = *drag_count;
    } else if (*drag_from <= -2) {
        int craft_slot = -2 - *drag_from;
        if (craft_slot >= 0 && craft_slot < CRAFTING_GRID_SIZE && crafting_items[craft_slot] == 0) {
            crafting_items[craft_slot] = *drag_item;
            crafting_counts[craft_slot] = *drag_count;
        } else {
            int remaining = inventory_add(inv, counts, *drag_item, *drag_count);
            if (remaining > 0)
                world_drop_item(world, *drag_item, remaining, cam->position[0], cam->position[1] + 0.8f,
                                cam->position[2], cam->orientation[0], cam->orientation[2]);
        }
    } else {
        int remaining = inventory_add(inv, counts, *drag_item, *drag_count);
        if (remaining > 0)
            world_drop_item(world, *drag_item, remaining, cam->position[0], cam->position[1] + 0.8f,
                            cam->position[2], cam->orientation[0], cam->orientation[2]);
    }
    *drag_item = 0;
    *drag_count = 0;
    *drag_from = -1;
}

static void distribute_drag_at(const InvLayout* layout, float mx, float my,
                               int* inv, int* counts, int* crafting_items, int* crafting_counts,
                               int* drag_item, int* drag_count, int* drag_from,
                               int* seen_inventory, int* seen_crafting) {
    if (!*drag_item) return;
    int slot = inventory_slot_at(layout, mx, my);
    if (slot >= 0 && !seen_inventory[slot]) {
        if (slot == *drag_from) return;
        if (inv[slot] != 0 && (inv[slot] != *drag_item || item_get(*drag_item)->is_bucket ||
                               counts[slot] >= item_stack_limit(*drag_item))) return;
        seen_inventory[slot] = 1;
    } else {
        int craft_slot = crafting_slot_at(layout, mx, my);
        if (craft_slot < 0 || seen_crafting[craft_slot]) return;
        if (*drag_from <= -2 && craft_slot == -2 - *drag_from) return;
        if (crafting_items[craft_slot] != 0 &&
            (crafting_items[craft_slot] != *drag_item ||
             crafting_counts[craft_slot] >= item_stack_limit(*drag_item))) return;
        seen_crafting[craft_slot] = 1;
    }

    int total = *drag_count;
    int slot_count = 0;
    for (int i = 0; i < INV_SIZE; i++) {
        if (!seen_inventory[i]) continue;
        slot_count++;
        if (inv[i] == *drag_item) total += counts[i];
    }
    for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
        if (!seen_crafting[i]) continue;
        slot_count++;
        if (crafting_items[i] == *drag_item) total += crafting_counts[i];
    }
    if (slot_count <= 0) return;
    int limit = item_stack_limit(*drag_item);
    int each = total / slot_count;
    int remainder = total % slot_count;
    int assigned = 0;
    int ordinal = 0;
    for (int i = 0; i < INV_SIZE; i++) {
        if (!seen_inventory[i]) continue;
        int amount = each + (ordinal < remainder ? 1 : 0);
        if (amount > limit) amount = limit;
        inv[i] = amount > 0 ? *drag_item : 0;
        counts[i] = amount;
        assigned += amount;
        ordinal++;
    }
    for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
        if (!seen_crafting[i]) continue;
        int amount = each + (ordinal < remainder ? 1 : 0);
        if (amount > limit) amount = limit;
        crafting_items[i] = amount > 0 ? *drag_item : 0;
        crafting_counts[i] = amount;
        assigned += amount;
        ordinal++;
    }
    *drag_count = total - assigned;
    if (*drag_count < 0) *drag_count = 0;
}

static void move_stack_to_crafting(int* inv, int* counts, int inventory_slot,
                                   int* crafting_items, int* crafting_counts) {
    int item = inv[inventory_slot];
    int amount = counts[inventory_slot];
    if (!item || amount <= 0) return;
    for (int i = 0; i < CRAFTING_GRID_SIZE && amount > 0; i++) {
        if (crafting_items[i] == 0) {
            crafting_items[i] = item;
            crafting_counts[i] = amount;
            amount = 0;
        } else if (crafting_items[i] == item && !item_get(item)->is_bucket) {
            int space = item_stack_limit(item) - crafting_counts[i];
            int moved = amount < space ? amount : space;
            crafting_counts[i] += moved;
            amount -= moved;
        }
    }
    counts[inventory_slot] = amount;
    if (amount == 0) inv[inventory_slot] = 0;
}

static int save_player_position(const char* path, const Camera* cam) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(cam->position, sizeof(float), 3, f);
    int closed = fclose(f) == 0;
    return written == 3 && closed;
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

static void day_cycle_sun_dir(float day_time, float day_length, float* dir_x, float* dir_y, float* dir_z) {
    const float tilt_cos = 0.86603f, tilt_sin = 0.5f;
    float cycle_angle = day_time / day_length * 6.28318530718f;
    *dir_x = cosf(cycle_angle);
    *dir_y = sinf(cycle_angle) * tilt_cos;
    *dir_z = sinf(cycle_angle) * tilt_sin;
}

static void day_cycle_shadow_steps(float day_time, float day_length, float* step_x, float* step_z) {
    float dx, dy, dz;
    day_cycle_sun_dir(day_time, day_length, &dx, &dy, &dz);
    if (dy < 0.0f) { dx = -dx; dy = -dy; dz = -dz; }
    if (dy < 0.15f) dy = 0.15f;
    *step_x = clampf_local(dx / dy, -1.5f, 1.5f);
    *step_z = clampf_local(dz / dy, -1.5f, 1.5f);
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
    int soft_lighting;
    int day_night_cycle;
    float day_time;
    int anti_aliasing;
} Settings;

static void load_settings(const char* path, Settings* settings) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    int fps_cap = settings->fps_cap;
    float render_distance_chunks = settings->render_distance_chunks;
    int gravity_enabled = settings->gravity_enabled;
    int soft_lighting = settings->soft_lighting;
    int day_night_cycle = settings->day_night_cycle;
    float day_time = settings->day_time;
    int anti_aliasing = settings->anti_aliasing;
    if (fscanf(f, "%d %f %d", &fps_cap, &render_distance_chunks, &gravity_enabled) == 3) {
        settings->fps_cap = fps_cap;
        settings->render_distance_chunks = render_distance_chunks;
        settings->gravity_enabled = gravity_enabled ? 1 : 0;
        if (fscanf(f, "%d", &soft_lighting) == 1) {
            settings->soft_lighting = soft_lighting ? 1 : 0;
            if (fscanf(f, "%d", &day_night_cycle) == 1) {
                settings->day_night_cycle = day_night_cycle ? 1 : 0;
                if (fscanf(f, "%f", &day_time) == 1) {
                    settings->day_time = day_time;
                    if (fscanf(f, "%d", &anti_aliasing) == 1)
                        settings->anti_aliasing = anti_aliasing ? 1 : 0;
                }
            }
        }
    }
    fclose(f);
}

static void save_settings(const char* path, const Settings* settings) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d %.3f %d %d %d %.2f %d\n", settings->fps_cap, settings->render_distance_chunks, settings->gravity_enabled ? 1 : 0, settings->soft_lighting ? 1 : 0, settings->day_night_cycle ? 1 : 0, settings->day_time, settings->anti_aliasing ? 1 : 0);
    fclose(f);
}

static void load_world_time(const char* path, float* day_time) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fread(day_time, sizeof(*day_time), 1, f);
    fclose(f);
}

static int save_world_time(const char* path, float day_time) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(&day_time, sizeof(day_time), 1, f);
    int closed = fclose(f) == 0;
    return written == 1 && closed;
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
    for (int i = 0; i < world->falling_count; i++) {
        FallingBlock* f = &world->falling[i];
        if (!f->started) continue;
        float bx0 = (float)f->ix, bx1 = bx0 + 1.0f;
        float by0 = f->y,         by1 = by0 + 1.0f;
        float bz0 = (float)f->iz, bz1 = bz0 + 1.0f;
        if (max_x >= bx0 && min_x <= bx1 &&
            max_y >= by0 && min_y <= by1 &&
            max_z >= bz0 && min_z <= bz1)
            return 1;
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
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7,
};

static void draw_sky_body(GLuint program, GLuint vao, GLuint vbo, GLint u_cam, GLint u_model, GLint u_color,
                          mat4 cam_matrix, vec3 cam_pos, float dir_x, float dir_y, float dir_z,
                          float half_size, float r, float g, float b) {
    if (dir_y < -0.12f) return;
    const float dist = 380.0f;
    float cx = cam_pos[0] + dir_x * dist;
    float cy = cam_pos[1] + dir_y * dist;
    float cz = cam_pos[2] + dir_z * dist;
    float ax = -dir_z, ay = 0.0f, az = dir_x;
    float alen = sqrtf(ax*ax + az*az);
    if (alen < 0.0001f) { ax = 0.0f; az = 1.0f; alen = 1.0f; }
    ax /= alen; az /= alen;
    float bx = ay*dir_z - az*dir_y;
    float by = az*dir_x - ax*dir_z;
    float bz = ax*dir_y - ay*dir_x;
    float h = half_size;
    float v[18] = {
        cx-(ax+bx)*h, cy-(ay+by)*h, cz-(az+bz)*h,
        cx+(ax-bx)*h, cy+(ay-by)*h, cz+(az-bz)*h,
        cx+(ax+bx)*h, cy+(ay+by)*h, cz+(az+bz)*h,
        cx-(ax+bx)*h, cy-(ay+by)*h, cz-(az+bz)*h,
        cx+(ax+bx)*h, cy+(ay+by)*h, cz+(az+bz)*h,
        cx-(ax-bx)*h, cy-(ay-by)*h, cz-(az-bz)*h,
    };
    mat4 ident;
    glm_mat4_identity(ident);
    glUseProgram(program);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glUniformMatrix4fv(u_cam, 1, GL_FALSE, (float*)cam_matrix);
    glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)ident);
    glUniform4f(u_color, r, g, b, 1.0f);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void ensure_msaa_targets(GLuint* fbo, GLuint* color_rbo, GLuint* depth_rbo, int* cur_w, int* cur_h, int w, int h) {
    if (*fbo && *cur_w == w && *cur_h == h) return;
    if (*fbo) {
        glDeleteFramebuffers(1, fbo);
        glDeleteRenderbuffers(1, color_rbo);
        glDeleteRenderbuffers(1, depth_rbo);
        *fbo = 0;
    }
    if (w <= 0 || h <= 0) return;
    GLint max_samples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    int samples = max_samples < 4 ? max_samples : 4;
    if (samples < 2) { *cur_w = 0; *cur_h = 0; return; }
    glGenFramebuffers(1, fbo);
    glGenRenderbuffers(1, color_rbo);
    glGenRenderbuffers(1, depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, *color_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, *depth_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *color_rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depth_rbo);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    GLint actual_samples = 0;
    glBindRenderbuffer(GL_RENDERBUFFER, *color_rbo);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &actual_samples);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE || actual_samples < 2) {
        glDeleteFramebuffers(1, fbo);
        glDeleteRenderbuffers(1, color_rbo);
        glDeleteRenderbuffers(1, depth_rbo);
        *fbo = 0; *cur_w = 0; *cur_h = 0;
        return;
    }
    *cur_w = w; *cur_h = h;
}

int main(void) {
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
#ifdef SDL_HINT_MOUSE_RELATIVE_MODE_CENTER
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "0");
#endif
#ifdef SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP, "0");
#endif
    SDL_Init(SDL_INIT_VIDEO);
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        g_wayland_video_driver = (video_driver && strcmp(video_driver, "wayland") == 0) ? 1 : 0;
        g_use_relative_mouse = g_wayland_video_driver ? 0 : 1;
        {
            const char* force_relative = getenv("T400CRAFT_FORCE_RELATIVE");
            if (force_relative && strcmp(force_relative, "1") == 0)
                g_use_relative_mouse = 1;
        }
    }
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("t400craft",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(0);
    glewInit();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    MenuResult menu_result;
menu_start:
    menu_run(window, &menu_result);
    if (menu_result.quit) {
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
    capture_relative_mouse(window);
    int drawable_w = (int)width, drawable_h = (int)height;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    glViewport(0, 0, drawable_w, drawable_h);

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
    GLint u_light_dir     = glGetUniformLocation(shaderProgram, "light_dir");
    GLint u_light_ambient = glGetUniformLocation(shaderProgram, "light_ambient");
    GLint u_light_diffuse = glGetUniformLocation(shaderProgram, "light_diffuse");

    char* cutVertSrc = load_file("src/Shaders/default.vert");
    char* cutFragSrc = load_file("src/Shaders/cutout.frag");
    GLuint cutVertexShader   = compile_shader_source(GL_VERTEX_SHADER,   cutVertSrc);
    GLuint cutFragmentShader = compile_shader_source(GL_FRAGMENT_SHADER, cutFragSrc);
    free(cutVertSrc); free(cutFragSrc);
    GLuint cutoutProgram = glCreateProgram();
    glAttachShader(cutoutProgram, cutVertexShader);
    glAttachShader(cutoutProgram, cutFragmentShader);
    glBindAttribLocation(cutoutProgram, 0, "aPos");
    glBindAttribLocation(cutoutProgram, 1, "aColor");
    glBindAttribLocation(cutoutProgram, 2, "aTexCoord");
    glBindAttribLocation(cutoutProgram, 3, "aTexLayer");
    glLinkProgram(cutoutProgram);
    glDeleteShader(cutVertexShader);
    glDeleteShader(cutFragmentShader);

    GLint u_cut_camMatrix     = glGetUniformLocation(cutoutProgram, "camMatrix");
    GLint u_cut_model         = glGetUniformLocation(cutoutProgram, "model");
    GLint u_cut_fog_color     = glGetUniformLocation(cutoutProgram, "fog_color");
    GLint u_cut_fog_start     = glGetUniformLocation(cutoutProgram, "fog_start");
    GLint u_cut_fog_end       = glGetUniformLocation(cutoutProgram, "fog_end");
    GLint u_cut_tex0          = glGetUniformLocation(cutoutProgram, "tex0");
    GLint u_cut_light_dir     = glGetUniformLocation(cutoutProgram, "light_dir");
    GLint u_cut_light_ambient = glGetUniformLocation(cutoutProgram, "light_ambient");
    GLint u_cut_light_diffuse = glGetUniformLocation(cutoutProgram, "light_diffuse");

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

    GLuint skyVAO, skyVBO;
    glGenVertexArrays(1, &skyVAO); glGenBuffers(1, &skyVBO);
    glBindVertexArray(skyVAO); glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*18, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    GLuint buttonVAO, buttonVBO;
    glGenVertexArrays(1, &buttonVAO); glGenBuffers(1, &buttonVBO);
    glBindVertexArray(buttonVAO); glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*24, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float))); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    World* world = malloc(sizeof(World));
    if (!world) { SDL_Quit(); return 1; }
    world_init(world, 0, 0, menu_result.world_dir, menu_result.seed, menu_result.natural);

    char player_path[320], inventory_path[320], world_time_path[320];
    snprintf(player_path, sizeof(player_path), "%s/player.bin", menu_result.world_dir);
    snprintf(inventory_path, sizeof(inventory_path), "%s/inventory.bin", menu_result.world_dir);
    snprintf(world_time_path, sizeof(world_time_path), "%s/time.bin", menu_result.world_dir);

    Mesh sand_cube_mesh = chunk_mesh_build_block(BLOCK_SAND);
    Mesh gravel_cube_mesh = chunk_mesh_build_block(BLOCK_GRAVEL);
    Mesh dropped_meshes[ITEM_COUNT] = {0};
    for (int i = 1; i < ITEM_COUNT; i++)
        if (!item_defs[i].is_bucket && item_defs[i].block != BLOCK_AIR)
            dropped_meshes[i] = chunk_mesh_build_block(item_defs[i].block);

    //location of textures for blocks
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
        "src/textures/stone.png",
        "src/textures/stone_bricks.png",
        "src/textures/smooth_stone.png",
        "src/textures/obsidian.png",
        "src/textures/sand.png",
        "src/textures/gravel.png",
        "src/textures/grass_path_side.png",
        "src/textures/grass_path_top.png",
        "src/textures/end_stone.png",
        "src/textures/end_stone_bricks.png",
        "src/textures/purple_stained_glass.png",
        "src/textures/blue_stained_glass.png",
        "src/textures/green_stained_glass.png",
        "src/textures/red_stained_glass.png",
    };
    GLuint texture = load_texture_array(world_textures, 24);

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
        unsigned char* hb = stbi_load(item_defs[i].texture_path, &hw2, &hh2, &hch2, 4);
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
    int inventory_counts[INV_SIZE] = {0};
    int hotbar_items = ITEM_COUNT - 1;
    if (hotbar_items > INV_SIZE - INV_HOTBAR_START) hotbar_items = INV_SIZE - INV_HOTBAR_START;
    for (int i = 1; i <= hotbar_items; i++) {
        inventory[INV_HOTBAR_START + i - 1] = i;
        inventory_counts[INV_HOTBAR_START + i - 1] = 1;
    }
    load_inventory(inventory_path, inventory, inventory_counts);
    int selected_slot = 0;
    int inventory_open = 0;
    int drag_item = 0, drag_count = 0, drag_from = -1;
    int drag_release_requested = 0;
    int drag_mouse_down = 0;
    int drag_seen_inventory[INV_SIZE] = {0};
    int drag_seen_crafting[CRAFTING_GRID_SIZE] = {0};
    int crafting_items[CRAFTING_GRID_SIZE] = {0};
    int crafting_counts[CRAFTING_GRID_SIZE] = {0};

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
    camera_init(&cam, drawable_w, drawable_h, start_pos);

    {
        FILE* pf = fopen(player_path, "rb");
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

    int running = 1, paused = 0, exit_to_menu = 0;
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
    float autosave_timer=0.0f;
    float save_status_timer=0.0f;
    int save_status=0;
    int fps_cap = 120;                          //standard setting at start
    float render_distance_chunks = 6.0f;        //standard setting at start
    float reach_distance = 7.5f;                //standard setting at start
    int paused_drag_slider = 0;
    int soft_lighting = 1;                      //standard setting at start
    int day_night_cycle = 1;                    //standard setting at start
    int anti_aliasing = 1;                      //standard setting at start
    int stream_frame_toggle = 0;
    int mouse_abs_valid = 0;
    int mouse_abs_last_x = 0;
    int mouse_abs_last_y = 0;
    GLuint msaa_fbo = 0, msaa_color_rbo = 0, msaa_depth_rbo = 0;
    int msaa_w = 0, msaa_h = 0;
    const float DAY_LENGTH_SECONDS = 1200.0f;   //length of a day-night-cycle in seconds (20 minutes by default)
    const float DAY_TIME_STATIC = 182.44f;
    float day_time = DAY_LENGTH_SECONDS * 0.25f;
    Uint32 last_time = SDL_GetTicks();
    Uint64 fps_perf_freq = SDL_GetPerformanceFrequency();
    Uint64 fps_frame_start_counter = SDL_GetPerformanceCounter();
    double fps_next_deadline_seconds = (double)fps_frame_start_counter / (double)fps_perf_freq;
    Settings settings = { fps_cap, render_distance_chunks, gravity_enabled, soft_lighting, day_night_cycle, day_time, anti_aliasing };
    load_settings("Savefiles/settings.cfg", &settings);
    fps_cap = settings.fps_cap;
    render_distance_chunks = settings.render_distance_chunks;
    gravity_enabled = settings.gravity_enabled;
    soft_lighting = settings.soft_lighting;
    day_night_cycle = settings.day_night_cycle;
    anti_aliasing = settings.anti_aliasing;
    load_world_time(world_time_path, &day_time);
    if (!(day_time >= 0.0f && day_time < DAY_LENGTH_SECONDS)) day_time = DAY_LENGTH_SECONDS * 0.25f;
    world_set_stream_radius(world, (int)ceilf(render_distance_chunks) + 1);
    chunk_mesh_set_soft_lighting(soft_lighting);
    chunk_mesh_set_directional_lighting(1);
    int last_shadow_bucket;
    {
        float et = day_night_cycle ? day_time : DAY_TIME_STATIC;
        float ssx, ssz;
        day_cycle_shadow_steps(et, DAY_LENGTH_SECONDS, &ssx, &ssz);
        chunk_mesh_set_shadow_dir(ssx, ssz);
        last_shadow_bucket = (int)(et / DAY_LENGTH_SECONDS * 16.0f);
    }

    capture_relative_mouse(window);

    while (running) {
        int break_requested=0, place_requested=0;
        int frame_mouse_dx = 0, frame_mouse_dy = 0;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_F3)
                show_fps = !show_fps;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
                event.key.keysym.sym == SDLK_q && !paused) {
                int amount = (SDL_GetModState() & KMOD_CTRL) ? INT_MAX : 1;
                if (inventory_open) {
                    int mx = 0, my = 0;
                    SDL_GetMouseState(&mx, &my);
                    float drawable_x = 0.0f, drawable_y = 0.0f;
                    ui_window_to_drawable(window, mx, my, &drawable_x, &drawable_y);
                    int sw = 0, sh = 0;
                    SDL_GL_GetDrawableSize(window, &sw, &sh);
                    InvLayout layout;
                    inventory_layout(sw, sh, &layout);
                    int slot = inventory_slot_at(&layout, drawable_x, drawable_y);
                    if (slot >= 0) {
                        if (amount == INT_MAX) amount = inventory_counts[slot];
                        drop_inventory_amount(world, &cam, inventory, inventory_counts, slot, amount);
                    } else {
                        int craft_slot = crafting_slot_at(&layout, drawable_x, drawable_y);
                        if (craft_slot >= 0 && crafting_items[craft_slot] != 0) {
                            if (amount == INT_MAX) amount = crafting_counts[craft_slot];
                            world_drop_item(world, crafting_items[craft_slot], amount,
                                cam.position[0], cam.position[1] + 0.8f, cam.position[2],
                                cam.orientation[0], cam.orientation[2]);
                            crafting_counts[craft_slot] -= amount;
                            if (crafting_counts[craft_slot] <= 0) crafting_items[craft_slot] = 0;
                        }
                    }
                } else {
                    int slot = INV_HOTBAR_START + selected_slot;
                    if (amount == INT_MAX) amount = inventory_counts[slot];
                    drop_inventory_amount(world, &cam, inventory, inventory_counts, slot, amount);
                }
            }
            if (event.type == SDL_MOUSEWHEEL && !paused && !inventory_open) {
                selected_slot = (selected_slot - event.wheel.y % 9 + 9) % 9;
            }
            if (event.type == SDL_MOUSEWHEEL && !paused && inventory_open) {
                palette_scroll -= event.wheel.y;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && !paused && !inventory_open) {
                SDL_Keycode k = event.key.keysym.sym;
                if (k >= SDLK_1 && k <= SDLK_9) selected_slot = k - SDLK_1;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_e && !paused) {
                inventory_open = !inventory_open;
                if (inventory_open) {
                    mouse_right_held = 0;
                    mouse_abs_valid = 0;
                    release_relative_mouse(window);
                } else {
                    finish_drag(world, &cam, inventory, inventory_counts, crafting_items, crafting_counts,
                                &drag_item, &drag_count, &drag_from);
                    return_crafting_to_inventory(crafting_items, crafting_counts, inventory, inventory_counts);
                    mouse_abs_valid = 0;
                    capture_relative_mouse(window);
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                if (inventory_open) {
                    inventory_open = 0;
                    finish_drag(world, &cam, inventory, inventory_counts, crafting_items, crafting_counts,
                                &drag_item, &drag_count, &drag_from);
                    return_crafting_to_inventory(crafting_items, crafting_counts, inventory, inventory_counts);
                    mouse_abs_valid = 0;
                    capture_relative_mouse(window);
                } else {
                    paused = !paused;
                    paused_drag_slider = 0;
                    if (paused) {
                        mouse_abs_valid = 0;
                        release_relative_mouse(window);
                    } else {
                        mouse_abs_valid = 0;
                        capture_relative_mouse(window);
                    }
                }
            } else if (!paused && event.type == SDL_KEYDOWN && event.key.repeat == 0
                       && event.key.keysym.sym == SDLK_SPACE && gravity_enabled) {
                (void)0;
            } else if (paused && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                float menu_w=360.0f, row_h=64.0f, row_gap=72.0f;
                float menu_x=((float)sw-menu_w)*0.5f;
                float continue_y=(float)sh*0.17f;
                float slider_y1=continue_y+row_h+108.0f;
                float slider_y2=slider_y1+row_gap;
                float button_y1=slider_y2+row_gap;
                float button_y2=button_y1+row_gap;
                float button_y3=button_y2+row_gap;
                float button_y4=button_y3+row_gap;
                float button_y5=button_y4+row_gap;
                float mouse_x=0.0f, mouse_y=0.0f;
                ui_window_to_drawable(window, event.button.x, event.button.y, &mouse_x, &mouse_y);
                if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, continue_y, menu_w, row_h)) {
                    paused=0; capture_relative_mouse(window);
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, slider_y1, menu_w, row_h)) {
                    paused_drag_slider = 1;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, slider_y2, menu_w, row_h)) {
                    paused_drag_slider = 2;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y1, menu_w, row_h)) {
                    anti_aliasing = !anti_aliasing;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y2, menu_w, row_h)) {
                    gravity_enabled=!gravity_enabled; if(!gravity_enabled) vel_y=0.0f;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y3, menu_w, row_h)) {
                    soft_lighting = !soft_lighting;
                    chunk_mesh_set_soft_lighting(soft_lighting);
                    for (int i = 0; i < WORLD_SLOTS; i++)
                        if (world->slots[i].loaded) world->slots[i].mesh_dirty = 1;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y4, menu_w, row_h)) {
                    day_night_cycle = !day_night_cycle;
                } else if (point_in_rect((int)mouse_x, (int)mouse_y, menu_x, button_y5, menu_w, row_h)) {
                    world_save_all_dirty(world);
                    running = 0;
                    exit_to_menu = 1;
                }
                if (paused_drag_slider == 1 || paused_drag_slider == 2) {
                    float t = clampf_local((mouse_x - menu_x) / menu_w, 0.0f, 1.0f);
                    if (paused_drag_slider == 1) {
                        fps_cap = 30 + (int)(t * 1970.0f + 0.5f);
                        fps_next_deadline_seconds = (double)SDL_GetPerformanceCounter() / (double)fps_perf_freq;
                    } else {
                        render_distance_chunks = 4.0f + t * 28.0f;
                        world_set_stream_radius(world, (int)ceilf(render_distance_chunks) + 1);
                    }
                }
            } else if (paused && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                paused_drag_slider = 0;
            } else if (!paused && inventory_open && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                InvLayout inv_layout; inventory_layout(sw, sh, &inv_layout);
                float mx=0.0f, my=0.0f;
                ui_window_to_drawable(window, event.button.x, event.button.y, &mx, &my);
                int palette_item = creative_palette_at(&inv_layout, mx, my);
                int craft_slot = crafting_slot_at(&inv_layout, mx, my);
                int slot = inventory_slot_at(&inv_layout, mx, my);
                if (drag_item) {
                    drag_mouse_down = 1;
                    drag_release_requested = 1;
                } else if (crafting_result_at(&inv_layout, mx, my)) {
                    craft_one(crafting_items, crafting_counts, inventory, inventory_counts);
                } else if (palette_item) {
                    int palette_amount = (SDL_GetModState() & KMOD_CTRL) ?
                        item_stack_limit(palette_item) : 1;
                    inventory_add(inventory, inventory_counts, palette_item, palette_amount);
                } else if (craft_slot >= 0 && crafting_items[craft_slot] != 0) {
                    memset(drag_seen_inventory, 0, sizeof(drag_seen_inventory));
                    memset(drag_seen_crafting, 0, sizeof(drag_seen_crafting));
                    drag_item = crafting_items[craft_slot];
                    drag_count = crafting_counts[craft_slot];
                    drag_from = -2 - craft_slot;
                    drag_mouse_down = 1;
                    crafting_items[craft_slot] = 0;
                    crafting_counts[craft_slot] = 0;
                } else if (slot >= 0 && inventory[slot] != 0) {
                    if (SDL_GetModState() & KMOD_SHIFT) {
                        move_stack_to_crafting(inventory, inventory_counts, slot,
                                               crafting_items, crafting_counts);
                    } else if (SDL_GetModState() & KMOD_CTRL) {
                        inventory_shift_click(inventory, inventory_counts, slot);
                    } else {
                        memset(drag_seen_inventory, 0, sizeof(drag_seen_inventory));
                        memset(drag_seen_crafting, 0, sizeof(drag_seen_crafting));
                        drag_item = inventory[slot];
                        drag_count = inventory_counts[slot];
                        drag_from = slot;
                        drag_mouse_down = 1;
                        inventory[slot] = 0;
                        inventory_counts[slot] = 0;
                    }
                }
            } else if (!paused && inventory_open && event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                if (drag_item && drag_release_requested) {
                    drag_release_requested = 0;
                    if (drag_count <= 0) {
                        drag_item = 0;
                        drag_mouse_down = 0;
                        drag_from = -1;
                        continue;
                    }
                    int sw=0, sh=0; SDL_GL_GetDrawableSize(window, &sw, &sh);
                    InvLayout inv_layout; inventory_layout(sw, sh, &inv_layout);
                    float mx=0.0f, my=0.0f;
                    ui_window_to_drawable(window, event.button.x, event.button.y, &mx, &my);
                    int craft_slot = crafting_slot_at(&inv_layout, mx, my);
                    int slot = inventory_slot_at(&inv_layout, mx, my);
                    int palette_item = creative_palette_at(&inv_layout, mx, my);
                    int over_result = crafting_result_at(&inv_layout, mx, my);
                    if (craft_slot >= 0) {
                        if (crafting_items[craft_slot] == 0) {
                            crafting_items[craft_slot] = drag_item;
                            crafting_counts[craft_slot] = drag_count;
                            drag_item = 0; drag_count = 0; drag_from = -1;
                        } else if (crafting_items[craft_slot] == drag_item && !item_get(drag_item)->is_bucket) {
                            int space = item_stack_limit(drag_item) - crafting_counts[craft_slot];
                            int moved = drag_count < space ? drag_count : space;
                            crafting_counts[craft_slot] += moved;
                            drag_count -= moved;
                            if (drag_count <= 0) {
                                drag_item = 0; drag_count = 0; drag_from = -1;
                            } else {
                                finish_drag(world, &cam, inventory, inventory_counts,
                                            crafting_items, crafting_counts,
                                            &drag_item, &drag_count, &drag_from);
                            }
                        } else {
                            finish_drag(world, &cam, inventory, inventory_counts,
                                        crafting_items, crafting_counts,
                                        &drag_item, &drag_count, &drag_from);
                        }
                    } else if (slot >= 0) {
                        if (inventory[slot] == 0) {
                            inventory[slot] = drag_item;
                            inventory_counts[slot] = drag_count;
                            drag_item = 0; drag_count = 0; drag_from = -1;
                        } else if (inventory[slot] == drag_item && !item_get(drag_item)->is_bucket) {
                            int limit = item_stack_limit(drag_item);
                            int space = limit - inventory_counts[slot];
                            int moved = drag_count < space ? drag_count : space;
                            inventory_counts[slot] += moved;
                            drag_count -= moved;
                            if (drag_count <= 0) {
                                drag_item = 0; drag_count = 0; drag_from = -1;
                            } else {
                                inventory_put_back(inventory, inventory_counts, &drag_item, &drag_count, &drag_from);
                            }
                        } else if (drag_from >= 0) {
                            int old_item = inventory[slot];
                            int old_count = inventory_counts[slot];
                            inventory[slot] = drag_item;
                            inventory_counts[slot] = drag_count;
                            inventory[drag_from] = old_item;
                            inventory_counts[drag_from] = old_count;
                            drag_item = 0;
                            drag_count = 0;
                            drag_from = -1;
                        } else {
                            inventory_put_back(inventory, inventory_counts, &drag_item, &drag_count, &drag_from);
                        }
                    } else if (craft_slot < 0 && slot < 0 && !palette_item && !over_result) {
                        world_drop_item(world, drag_item, drag_count,
                            cam.position[0], cam.position[1] + 0.8f, cam.position[2],
                            cam.orientation[0], cam.orientation[2]);
                        drag_item = 0; drag_count = 0; drag_from = -1;
                    } else {
                        for (int i = 0; i < INV_MAIN_COUNT && drag_item; i++)
                            if (inventory[i] == 0) {
                                inventory[i] = drag_item;
                                inventory_counts[i] = drag_count;
                                drag_item = 0;
                                drag_count = 0;
                                drag_from = -1;
                            }
                        inventory_put_back(inventory, inventory_counts, &drag_item, &drag_count, &drag_from);
                    }
                } else {
                    drag_mouse_down = 0;
                }
            } else if (!paused && !inventory_open && event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT)       break_requested=1;
                if (event.button.button == SDL_BUTTON_RIGHT)      mouse_right_held=1;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_RIGHT)      mouse_right_held=0;
            }

            if (!paused && !inventory_open && event.type == SDL_MOUSEMOTION) {
                if (g_use_relative_mouse) {
                    frame_mouse_dx += event.motion.xrel;
                    frame_mouse_dy += event.motion.yrel;
                } else {
                    int mx = event.motion.x;
                    int my = event.motion.y;
                    if (mouse_abs_valid) {
                        frame_mouse_dx += (mx - mouse_abs_last_x);
                        frame_mouse_dy += (my - mouse_abs_last_y);
                    }
                    mouse_abs_last_x = mx;
                    mouse_abs_last_y = my;
                    mouse_abs_valid = 1;
                }
            }
            if (!paused && inventory_open && drag_item && drag_mouse_down && event.type == SDL_MOUSEMOTION) {
                int sw = 0, sh = 0;
                SDL_GL_GetDrawableSize(window, &sw, &sh);
                InvLayout layout;
                inventory_layout(sw, sh, &layout);
                float mx = 0.0f, my = 0.0f;
                ui_window_to_drawable(window, event.motion.x, event.motion.y, &mx, &my);
                distribute_drag_at(&layout, mx, my, inventory, inventory_counts,
                                   crafting_items, crafting_counts, &drag_item, &drag_count, &drag_from,
                                   drag_seen_inventory, drag_seen_crafting);
            }

            if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int w,h; SDL_GL_GetDrawableSize(window,&w,&h);
                    glViewport(0,0,w,h); cam.width=w; cam.height=h;
                }
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    mouse_abs_valid = 0;
                    release_relative_mouse(window);
                }
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && !paused && !inventory_open) {
                    mouse_abs_valid = 0;
                    capture_relative_mouse(window);
                }
            }
        }

        int cx_player = (int)floorf(cam.position[0] / CHUNK_SIZE_X);
        int cz_player = (int)floorf(cam.position[2] / CHUNK_SIZE_Z);
        world_update_center(world, cx_player, cz_player);
        stream_frame_toggle ^= 1;
        world_stream_missing(world, stream_frame_toggle ? 1 : 0);

        {
            Uint64 rb_start = SDL_GetPerformanceCounter();
            int rebuilt = 0;
            for (int ring = 0; ring <= world->stream_radius; ring++) {
                for (int rdx = -ring; rdx <= ring; rdx++) {
                    for (int rdz = -ring; rdz <= ring; rdz++) {
                        if (abs(rdx) != ring && abs(rdz) != ring) continue;
                        WorldSlot* s = world_get_slot(world, cx_player + rdx, cz_player + rdz);
                        if (!s || (s->mesh_valid && !s->mesh_dirty)) continue;
                        world_rebuild_mesh(world, s->chunk.cx, s->chunk.cz);
                        rebuilt++;
                        double rb_elapsed = (double)(SDL_GetPerformanceCounter() - rb_start) / (double)fps_perf_freq;
                        if (rebuilt >= 1 && rb_elapsed >= 0.0025) goto rebuild_done;
                    }
                }
            }
            rebuild_done:;
        }

        int screen_w=0, screen_h=0;
        SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_time) * 0.001f;
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;
        if (save_status_timer > 0.0f) {
            save_status_timer -= dt;
            if (save_status_timer < 0.0f) save_status_timer = 0.0f;
        }

        if (day_night_cycle && !paused) {
            day_time += dt;
            if (day_time >= DAY_LENGTH_SECONDS) day_time -= DAY_LENGTH_SECONDS;
        }
        float effective_day_time = day_night_cycle ? day_time : DAY_TIME_STATIC;
        float sky_r, sky_g, sky_b;
        float sun_dir_x, sun_dir_y, sun_dir_z;
        float light_dx, light_dy, light_dz;
        float light_ambient, light_diffuse;
        day_cycle_sun_dir(effective_day_time, DAY_LENGTH_SECONDS, &sun_dir_x, &sun_dir_y, &sun_dir_z);
        int shadow_bucket = (int)(effective_day_time / DAY_LENGTH_SECONDS * 16.0f);
        if (shadow_bucket != last_shadow_bucket) {
            last_shadow_bucket = shadow_bucket;
            float ssx, ssz;
            day_cycle_shadow_steps(effective_day_time, DAY_LENGTH_SECONDS, &ssx, &ssz);
            chunk_mesh_set_shadow_dir(ssx, ssz);
            for (int i = 0; i < WORLD_SLOTS; i++) {
                WorldSlot* s = &world->slots[i];
                if (!s->loaded) continue;
                float swx = (float)(s->chunk.cx * CHUNK_SIZE_X);
                float swz = (float)(s->chunk.cz * CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], swx, swz, render_distance_chunks + 0.5f)) continue;
                s->mesh_dirty = 1;
            }
        }
        float sun_up = clampf_local(sun_dir_y * 6.0f, 0.0f, 1.0f);
        float moon_up = clampf_local(-sun_dir_y * 6.0f, 0.0f, 1.0f);
        light_ambient = 0.16f + 0.29f * sun_up + 0.10f * moon_up;
        if (sun_up >= moon_up) {
            light_dx = sun_dir_x; light_dy = sun_dir_y; light_dz = sun_dir_z;
            light_diffuse = 0.55f * sun_up;
        } else {
            light_dx = -sun_dir_x; light_dy = -sun_dir_y; light_dz = -sun_dir_z;
            light_diffuse = 0.18f * moon_up;
        }
        sky_r = 0.015f + (FOG_R - 0.015f) * sun_up;
        sky_g = 0.025f + (FOG_G - 0.025f) * sun_up;
        sky_b = 0.070f + (FOG_B - 0.070f) * sun_up;
        float horizon_glow = clampf_local(1.0f - fabsf(sun_dir_y) * 4.0f, 0.0f, 1.0f);
        sky_r = fminf(sky_r + horizon_glow * 0.30f, 1.0f);
        sky_g = fminf(sky_g + horizon_glow * 0.10f, 1.0f);

        if (anti_aliasing)
            ensure_msaa_targets(&msaa_fbo, &msaa_color_rbo, &msaa_depth_rbo, &msaa_w, &msaa_h, screen_w, screen_h);
        int use_msaa = anti_aliasing && msaa_fbo != 0;
        glBindFramebuffer(GL_FRAMEBUFFER, use_msaa ? msaa_fbo : 0);
        glClearColor(sky_r, sky_g, sky_b, 1.0f);
        if (use_msaa) glEnable(GL_MULTISAMPLE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(cutoutProgram);
        glUniform3f(u_cut_light_dir, light_dx, light_dy, light_dz);
        glUniform1f(u_cut_light_ambient, light_ambient);
        glUniform1f(u_cut_light_diffuse, light_diffuse);
        glUseProgram(shaderProgram);
        glUniform3f(u_light_dir, light_dx, light_dy, light_dz);
        glUniform1f(u_light_ambient, light_ambient);
        glUniform1f(u_light_diffuse, light_diffuse);

        float render_distance_effective_chunks = render_distance_chunks;
        float fog_end = fmaxf(render_distance_effective_chunks * (float)CHUNK_SIZE_X * 0.55f, 24.0f);
        float fog_start = fmaxf(fog_end * 0.30f, 8.0f);
        if (fog_start > fog_end - 6.0f) fog_start = fmaxf(fog_end - 6.0f, 0.0f);

        mat4 model; glm_mat4_identity(model);

        if (!paused) world_update_water(world, dt);
        if (!paused) world_update_gravity(world, dt);
        if (!paused) {
            world_update_dropped_items(world, dt);
            for (int i = 0; i < world->dropped_count; ) {
                DroppedItem* drop = &world->dropped[i];
                float dx = drop->x - cam.position[0];
                float dy = drop->y - cam.position[1];
                float dz = drop->z - cam.position[2];
                if (drop->pickup_delay <= 0.0f && dx * dx + dy * dy + dz * dz <= 2.25f) {
                    int remaining = inventory_add_pickup(inventory, inventory_counts, drop->item, drop->count);
                    if (remaining == 0) {
                        world->dropped[i] = world->dropped[--world->dropped_count];
                        continue;
                    }
                    drop->count = remaining;
                }
                i++;
            }
        }

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
            static const Uint8 locked_keys[SDL_NUM_SCANCODES];
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            if (inventory_open) keys = locked_keys;
            else camera_rotate(&cam, frame_mouse_dx, frame_mouse_dy);

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

            /* Survival mining drops belong behind this switch. Creative mode
             * still removes blocks, but does not create item entities. */
            if (break_requested && sel.hit) {
                Block* broken = world_get_block(world, sel.block_x, sel.block_y, sel.block_z);
                int dropped_item = broken ? item_for_block(broken->type) : 0;
                if (broken && item_hardness_for_block(broken->type) <= 1 &&
                    world_set_block(world, sel.block_x, sel.block_y, sel.block_z, BLOCK_AIR)) {
                    if (SURVIVAL_MODE)
                        world_drop_item(world, dropped_item, 1,
                            (float)sel.block_x + 0.5f, (float)sel.block_y + 0.65f,
                            (float)sel.block_z + 0.5f, cam.orientation[0], cam.orientation[2]);
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
                glUniform3f(u_fog_color, sky_r * 0.78f, sky_g * 0.86f, fminf(sky_b * 1.12f, 1.0f));
            else
                glUniform3f(u_fog_color, sky_r, sky_g, sky_b);
            glUniform1f(u_fog_start, fog_start);
            glUniform1f(u_fog_end,   fog_end);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glUniform1i(u_tex0, 0);
            glUseProgram(cutoutProgram);
            glUniformMatrix4fv(u_cut_camMatrix, 1, GL_FALSE, (float*)cam.camera_matrix);
            if (head_in_water)
                glUniform3f(u_cut_fog_color, sky_r * 0.78f, sky_g * 0.86f, fminf(sky_b * 1.12f, 1.0f));
            else
                glUniform3f(u_cut_fog_color, sky_r, sky_g, sky_b);
            glUniform1f(u_cut_fog_start, fog_start);
            glUniform1f(u_cut_fog_end,   fog_end);
            glUniform1i(u_cut_tex0, 0);
            glUseProgram(shaderProgram);

            draw_sky_body(selectionProgram, skyVAO, skyVBO, u_sel_cam, u_sel_model, u_sel_color,
                          cam.camera_matrix, cam.position, sun_dir_x, sun_dir_y, sun_dir_z, 26.0f, 1.0f, 0.97f, 0.80f);
            draw_sky_body(selectionProgram, skyVAO, skyVBO, u_sel_cam, u_sel_model, u_sel_color,
                          cam.camera_matrix, cam.position, -sun_dir_x, -sun_dir_y, -sun_dir_z, 18.0f, 0.72f, 0.76f, 0.85f);
            glUseProgram(shaderProgram);

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

            for (int i = 0; i < world->falling_count; i++) {
                FallingBlock* f = &world->falling[i];
                if (!f->started) continue;
                mat4 fall_model; glm_mat4_identity(fall_model);
                vec3 fall_offset = {(float)f->ix, f->y, (float)f->iz};
                glm_translate(fall_model, fall_offset);
                glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)fall_model);
                mesh_draw(f->type == BLOCK_GRAVEL ? &gravel_cube_mesh : &sand_cube_mesh);
            }

            for (int i = 0; i < world->dropped_count; i++) {
                DroppedItem* drop = &world->dropped[i];
                if (drop->item <= 0 || drop->item >= ITEM_COUNT ||
                    dropped_meshes[drop->item].index_count == 0 ||
                    block_transparent(item_defs[drop->item].block)) continue;
                mat4 drop_model; glm_mat4_identity(drop_model);
                vec3 drop_pos = {drop->x - 0.18f, drop->y - 0.18f, drop->z - 0.18f};
                glm_translate(drop_model, drop_pos);
                glm_rotate(drop_model, drop->yaw, (vec3){0.0f, 1.0f, 0.0f});
                glm_scale_uni(drop_model, 0.36f);
                glUniformMatrix4fv(u_model, 1, GL_FALSE, (float*)drop_model);
                mesh_draw(&dropped_meshes[drop->item]);
            }

            glUseProgram(cutoutProgram);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_TRUE);
            for (int i = 0; i < WORLD_SLOTS; i++) {
                WorldSlot* s = &world->slots[i];
                if (!s->loaded || !s->mesh_valid) continue;
                if (s->cutout_mesh.index_count == 0) continue;
                float wx = (float)(s->chunk.cx * CHUNK_SIZE_X);
                float wz = (float)(s->chunk.cz * CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if (!chunk_in_frustum(frustum, wx, wz)) continue;
                mat4 chunk_model; glm_mat4_identity(chunk_model);
                vec3 chunk_offset = {wx, 0.0f, wz};
                glm_translate(chunk_model, chunk_offset);
                glUniformMatrix4fv(u_cut_model, 1, GL_FALSE, (float*)chunk_model);
                mesh_draw(&s->cutout_mesh);
            }
            for (int i = 0; i < world->dropped_count; i++) {
                DroppedItem* drop = &world->dropped[i];
                if (drop->item <= 0 || drop->item >= ITEM_COUNT ||
                    dropped_meshes[drop->item].index_count == 0 ||
                    !block_transparent(item_defs[drop->item].block)) continue;
                mat4 drop_model; glm_mat4_identity(drop_model);
                vec3 drop_pos = {drop->x - 0.18f, drop->y - 0.18f, drop->z - 0.18f};
                glm_translate(drop_model, drop_pos);
                glm_rotate(drop_model, drop->yaw, (vec3){0.0f, 1.0f, 0.0f});
                glm_scale_uni(drop_model, 0.36f);
                glUniformMatrix4fv(u_cut_model, 1, GL_FALSE, (float*)drop_model);
                mesh_draw(&dropped_meshes[drop->item]);
            }
            glDisable(GL_BLEND);
            glUseProgram(shaderProgram);

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
                glUniform4f(u_sel_color, 1.0f, 1.0f, 1.0f, 0.35f);
                glLineWidth(1.5f);
                glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0); glEnable(GL_CULL_FACE); glDisable(GL_BLEND);
                glUseProgram(shaderProgram);
            }

            if (anti_aliasing && msaa_fbo != 0) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa_fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, screen_w, screen_h, 0, 0, screen_w, screen_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
                int coord_x = (int)floorf(cam.position[0]) - spawn_wx;
                int coord_y = (int)floorf(cam.position[1]);
                int coord_z = (int)floorf(cam.position[2]) - spawn_wz;
                char coord_str[64];
                char save_str[64];
                snprintf(coord_str, sizeof(coord_str), "x: %d y: %d z: %d", coord_x, coord_y, coord_z);
                if (save_status_timer > 0.0f) {
                    snprintf(save_str, sizeof(save_str), "%s", save_status == 1 ? "saved" : "saving failed");
                } else {
                    int seconds = (int)ceilf(60.0f - autosave_timer);
                    if (seconds < 0) seconds = 0;
                    snprintf(save_str, sizeof(save_str), "saving in: %d seconds", seconds);
                }
                float f3_vertices[8192];
                int f3_count = 0;
                build_text_vertices(coord_str, 8.0f, 8.0f, 1.8f, f3_vertices, &f3_count);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * f3_count, f3_vertices);
                glUniform2f(u_ui_screenSize,(float)screen_w,(float)screen_h);
                glUniform4f(u_ui_color,1,1,1,1);
                glDrawArrays(GL_TRIANGLES,0,f3_count/2);
                f3_count = 0;
                build_text_vertices(save_str, 8.0f, 24.0f, 1.8f, f3_vertices, &f3_count);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * f3_count, f3_vertices);
                glUniform4f(u_ui_color,1,1,1,0.8f);
                glDrawArrays(GL_TRIANGLES,0,f3_count/2);
                char fps_str[16]; int n=fps_display, pos=0;
                if(n==0){fps_str[pos++]='0';}
                else{char tmp[8];int tl=0;while(n>0){tmp[tl++]=(char)('0'+n%10);n/=10;}for(int fi=tl-1;fi>=0;fi--)fps_str[pos++]=tmp[fi];}
                fps_str[pos++]=' ';fps_str[pos++]='f';fps_str[pos++]='p';fps_str[pos++]='s';fps_str[pos]=0;
                float ftverts[4096]; int ftc=0;
                build_text_vertices(fps_str,8,40,1.8f,ftverts,&ftc);
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
                for (int si = 0; si < 9; si++) {
                    float sx2 = hx + si * (slot_size + pad);
                    draw_stack_count(uiProgram, uiVAO, uiVBO, u_ui_screenSize, u_ui_color,
                                     screen_w, screen_h, sx2, hy, slot_size,
                                     inventory_counts[INV_HOTBAR_START + si]);
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
                append_rect(rect, &rc,
                            inv_layout.x - panel_pad,
                            inv_layout.y - panel_pad,
                            grid_w + 2.0f * panel_pad,
                            grid_h + 2.0f * panel_pad);

                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * rc, rect);
                glUniform4f(u_ui_color, 0.12f, 0.12f, 0.12f, 0.92f);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                /* Creative inventory background */
                rc = 0;
                append_rect(rect, &rc,
                            inv_layout.palette_x - panel_pad,
                            inv_layout.craft_y - panel_pad,
                            inv_layout.x + grid_w - inv_layout.palette_x + 2.0f * panel_pad,
                            inv_layout.y + grid_h - inv_layout.craft_y + 2.0f * panel_pad);

                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * rc, rect);
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
                for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
                    float rx, ry;
                    crafting_slot_rect(&inv_layout, i, &rx, &ry);
                    rc = 0; append_rect(rect, &rc, rx, ry, inv_layout.cell, inv_layout.cell);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*rc, rect);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                {
                    float rx = inv_layout.output_x;
                    float ry = inv_layout.output_y;
                    rc = 0; append_rect(rect, &rc, rx, ry, inv_layout.cell, inv_layout.cell);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*rc, rect);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    float arrow[36];
                    int arrow_count = 0;
                    append_rect(arrow, &arrow_count, inv_layout.craft_x + 2.0f * (inv_layout.cell + inv_layout.pad),
                                inv_layout.output_y + inv_layout.cell * 0.46f,
                                inv_layout.output_x - inv_layout.craft_x - 2.0f * (inv_layout.cell + inv_layout.pad),
                                inv_layout.cell * 0.08f);
                    append_rect(arrow, &arrow_count, inv_layout.output_x - inv_layout.cell * 0.22f,
                                inv_layout.output_y + inv_layout.cell * 0.22f,
                                inv_layout.cell * 0.22f, inv_layout.cell * 0.08f);
                    append_rect(arrow, &arrow_count, inv_layout.output_x - inv_layout.cell * 0.22f,
                                inv_layout.output_y + inv_layout.cell * 0.70f,
                                inv_layout.cell * 0.22f, inv_layout.cell * 0.08f);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*arrow_count, arrow);
                    glDrawArrays(GL_TRIANGLES, 0, arrow_count / 2);
                }
                for (int i = 1; i < ITEM_COUNT; i++) {
                    float rx, ry;
                    creative_palette_rect(&inv_layout, i, &rx, &ry);
                    float hotbar_bottom = inv_layout.y + 3.0f * (inv_layout.cell + inv_layout.pad) + inv_layout.gap + inv_layout.cell;
                    if (ry < inv_layout.palette_y || ry + inv_layout.cell > hotbar_bottom) continue;
                    rc = 0; append_rect(rect, &rc, rx, ry, inv_layout.cell, inv_layout.cell);
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
                for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
                    GLuint itex = item_textures[crafting_items[i]];
                    if (!itex) continue;
                    float rx, ry; crafting_slot_rect(&inv_layout, i, &rx, &ry);
                    float ox = rx + (inv_layout.cell - inner) * 0.5f;
                    float oy = ry + (inv_layout.cell - inner) * 0.5f;
                    float iv[24] = {ox,oy,0,0, ox+inner,oy,1,0, ox+inner,oy+inner,1,1,
                                    ox,oy,0,0, ox+inner,oy+inner,1,1, ox,oy+inner,0,1};
                    glBindTexture(GL_TEXTURE_2D, itex);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                int recipe_index = item_find_recipe(crafting_items, crafting_counts, CRAFTING_GRID_SIZE);
                if (recipe_index >= 0) {
                    int result_item = item_recipes[recipe_index].result_item;
                    GLuint itex = item_textures[result_item];
                    if (itex) {
                        float rx = inv_layout.output_x;
                        float ry = inv_layout.output_y;
                        float ox = rx + (inv_layout.cell - inner) * 0.5f;
                        float oy = ry + (inv_layout.cell - inner) * 0.5f;
                        float iv[24] = {ox,oy,0,0, ox+inner,oy,1,0, ox+inner,oy+inner,1,1,
                                        ox,oy,0,0, ox+inner,oy+inner,1,1, ox,oy+inner,0,1};
                        glBindTexture(GL_TEXTURE_2D, itex);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                    }
                }
                for (int i = 1; i < ITEM_COUNT; i++) {
                    GLuint itex = item_textures[i];
                    if (!itex) continue;
                    float rx, ry; creative_palette_rect(&inv_layout, i, &rx, &ry);
                    float hotbar_bottom = inv_layout.y + 3.0f * (inv_layout.cell + inv_layout.pad) + inv_layout.gap + inv_layout.cell;
                    if (ry < inv_layout.palette_y || ry + inv_layout.cell > hotbar_bottom) continue;
                    float ox = rx + (inv_layout.cell - inner) * 0.5f;
                    float oy = ry + (inv_layout.cell - inner) * 0.5f;
                    float iv[24] = {ox,oy,0,0, ox+inner,oy,1,0, ox+inner,oy+inner,1,1,
                                    ox,oy,0,0, ox+inner,oy+inner,1,1, ox,oy+inner,0,1};
                    glBindTexture(GL_TEXTURE_2D, itex);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                for (int i = 0; i < INV_SIZE; i++) {
                    float rx, ry;
                    inventory_slot_rect(&inv_layout, i, &rx, &ry);
                    draw_stack_count(uiProgram, uiVAO, uiVBO, u_ui_screenSize, u_ui_color,
                                     screen_w, screen_h, rx, ry, inv_layout.cell, inventory_counts[i]);
                }
                for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
                    float rx, ry;
                    crafting_slot_rect(&inv_layout, i, &rx, &ry);
                    draw_stack_count(uiProgram, uiVAO, uiVBO, u_ui_screenSize, u_ui_color,
                                     screen_w, screen_h, rx, ry, inv_layout.cell, crafting_counts[i]);
                }
                if (recipe_index >= 0) {
                    draw_stack_count(uiProgram, uiVAO, uiVBO, u_ui_screenSize, u_ui_color,
                                     screen_w, screen_h, inv_layout.output_x, inv_layout.output_y,
                                     inv_layout.cell, item_recipes[recipe_index].result_count);
                }
                if (drag_item && drag_count > 0 && item_textures[drag_item]) {
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
                    glUseProgram(buttonProgram);
                    glBindVertexArray(buttonVAO);
                    glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
                    glUniform2f(u_btn_screenSize, (float)screen_w, (float)screen_h);
                    glActiveTexture(GL_TEXTURE3);
                    glUniform1i(u_btn_tex, 3);
                    glBindTexture(GL_TEXTURE_2D, item_textures[drag_item]);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(iv), iv);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    draw_stack_count(uiProgram, uiVAO, uiVBO, u_ui_screenSize, u_ui_color,
                                     screen_w, screen_h, mx - inner * 0.5f, my - inner * 0.5f,
                                     inner, drag_count);
                }
                glBindTexture(GL_TEXTURE_2D, 0);
                glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
                glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST);
            }

        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(sky_r, sky_g, sky_b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            float pry=cam.position[1];
            cam.position[1]+=crouching?CROUCH_EYE:STAND_EYE;
            camera_update(&cam,45.0f,0.1f,1000.0f);
            cam.position[1]=pry;
            glUniformMatrix4fv(u_camMatrix,1,GL_FALSE,(float*)cam.camera_matrix);
            glUniformMatrix4fv(u_view,1,GL_FALSE,(float*)cam.view);
            glUniformMatrix4fv(u_proj,1,GL_FALSE,(float*)cam.proj);
            glUniform3f(u_fog_color,sky_r,sky_g,sky_b);
            glUniform1f(u_fog_start,fog_start); glUniform1f(u_fog_end,fog_end);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
            glUniform1i(u_tex0,0);
            glUseProgram(cutoutProgram);
            glUniformMatrix4fv(u_cut_camMatrix,1,GL_FALSE,(float*)cam.camera_matrix);
            glUniform3f(u_cut_fog_color,sky_r,sky_g,sky_b);
            glUniform1f(u_cut_fog_start,fog_start); glUniform1f(u_cut_fog_end,fog_end);
            glUniform1i(u_cut_tex0,0);
            glUseProgram(shaderProgram);

            draw_sky_body(selectionProgram, skyVAO, skyVBO, u_sel_cam, u_sel_model, u_sel_color,
                          cam.camera_matrix, cam.position, sun_dir_x, sun_dir_y, sun_dir_z, 26.0f, 1.0f, 0.97f, 0.80f);
            draw_sky_body(selectionProgram, skyVAO, skyVBO, u_sel_cam, u_sel_model, u_sel_color,
                          cam.camera_matrix, cam.position, -sun_dir_x, -sun_dir_y, -sun_dir_z, 18.0f, 0.72f, 0.76f, 0.85f);
            glUseProgram(shaderProgram);

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

            glUseProgram(cutoutProgram);
            for (int i=0;i<WORLD_SLOTS;i++) {
                WorldSlot* s=&world->slots[i];
                if(!s->loaded||!s->mesh_valid) continue;
                if (s->cutout_mesh.index_count == 0) continue;
                float wx=(float)(s->chunk.cx*CHUNK_SIZE_X), wz=(float)(s->chunk.cz*CHUNK_SIZE_Z);
                if (!chunk_within_render_distance(cam.position[0], cam.position[2], wx, wz, render_distance_effective_chunks)) continue;
                if(!chunk_in_frustum(frustum,wx,wz)) continue;
                mat4 cm; glm_mat4_identity(cm);
                vec3 co={wx,0.0f,wz}; glm_translate(cm,co);
                glUniformMatrix4fv(u_cut_model,1,GL_FALSE,(float*)cm);
                mesh_draw(&s->cutout_mesh);
            }
            glUseProgram(shaderProgram);

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

            float panel_w=360.0f, row_h=64.0f, row_gap=72.0f, slider_h=12.0f, knob_w=14.0f;
            float panel_x=((float)screen_w-panel_w)*0.5f;
            float continue_y=(float)screen_h*0.17f;
            float slider_y1=continue_y+row_h+108.0f;
            float slider_y2=slider_y1+row_gap;
            float button_y1=slider_y2+row_gap;
            float button_y2=button_y1+row_gap;
            float button_y3=button_y2+row_gap;
            float button_y4=button_y3+row_gap;
            float button_y5=button_y4+row_gap;

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
            float button_ys[]={continue_y,button_y1,button_y2,button_y3,button_y4,button_y5};
            for(int b=0;b<6;b++){
                float by2=button_ys[b];
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
                                  anti_aliasing?"anti-aliasing: on":"anti-aliasing: off",
                                  gravity_enabled?"gravity: on":"gravity: off",
                                  soft_lighting?"lighting: soft":"lighting: hard",
                                  day_night_cycle?"day-night-cycle: on":"day-night-cycle: off",
                                  "save & exit" };
            float ypos[]={continue_y,button_y1,button_y2,button_y3,button_y4,button_y5};
            for(int b=0;b<6;b++){
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

        autosave_timer += dt;
        if (autosave_timer >= 60.0f) {
            autosave_timer -= 60.0f;
            int save_ok = save_player_position(player_path, &cam);
            save_ok = save_inventory(inventory_path, inventory, inventory_counts) && save_ok;
            save_ok = save_world_time(world_time_path, day_time) && save_ok;
            save_ok = world_save_all_dirty(world) && save_ok;
            save_status = save_ok ? 1 : 2;
            save_status_timer = 2.0f;
        }

        SDL_GL_SwapWindow(window);
        fps_next_deadline_seconds += 1.0 / (double)fps_cap;
        fps_frame_start_counter = SDL_GetPerformanceCounter();
    }

    save_player_position(player_path, &cam);

    settings.fps_cap = fps_cap;
    settings.render_distance_chunks = render_distance_chunks;
    settings.gravity_enabled = gravity_enabled;
    settings.soft_lighting = soft_lighting;
    settings.day_night_cycle = day_night_cycle;
    settings.day_time = day_time;
    settings.anti_aliasing = anti_aliasing;
    save_settings("Savefiles/settings.cfg", &settings);
    return_crafting_to_inventory(crafting_items, crafting_counts, inventory, inventory_counts);
    finish_drag(world, &cam, inventory, inventory_counts, crafting_items, crafting_counts,
                &drag_item, &drag_count, &drag_from);
    save_inventory(inventory_path, inventory, inventory_counts);
    save_world_time(world_time_path, day_time);

    world_save_all_dirty(world);
    world_free(world);
    mesh_delete(&sand_cube_mesh);
    mesh_delete(&gravel_cube_mesh);
    for (int i = 1; i < ITEM_COUNT; i++)
        if (dropped_meshes[i].index_count != 0) mesh_delete(&dropped_meshes[i]);
    if (msaa_fbo) {
        glDeleteFramebuffers(1, &msaa_fbo);
        glDeleteRenderbuffers(1, &msaa_color_rbo);
        glDeleteRenderbuffers(1, &msaa_depth_rbo);
    }
    glDeleteTextures(1,&texture); glDeleteTextures(1,&buttonTexture); glDeleteTextures(1,&crosshairTexture);
    for (int i = 0; i < ITEM_COUNT; i++) if (item_textures[i]) glDeleteTextures(1, &item_textures[i]);
    glDeleteProgram(shaderProgram); glDeleteProgram(cutoutProgram); glDeleteProgram(uiProgram); glDeleteProgram(buttonProgram); glDeleteProgram(selectionProgram);
    glDeleteBuffers(1,&uiVBO); glDeleteVertexArrays(1,&uiVAO);
    glDeleteBuffers(1,&buttonVBO); glDeleteVertexArrays(1,&buttonVAO);
    glDeleteBuffers(1,&selectionEBO); glDeleteBuffers(1,&selectionVBO); glDeleteVertexArrays(1,&selectionVAO);
    glDeleteBuffers(1,&skyVBO); glDeleteVertexArrays(1,&skyVAO);
    if (exit_to_menu) {
        free(world);
        release_relative_mouse(window);
        goto menu_start;
    }
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(world);
    return 0;
}