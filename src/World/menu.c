#include "menu.h"
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/stb_image.h"

#define MENU_MAX_WORLDS 32
#define MENU_SEED_MAX 10
#define MENU_NAME_MAX 20

typedef struct {
    char name[64];
    char dir[256];
} MenuWorld;

enum { MENU_SCREEN_MAIN, MENU_SCREEN_CREATE, MENU_SCREEN_RENAME };

static GLuint menu_compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static GLuint menu_create_program(const char* vs, const char* fs, int with_texcoord) {
    GLuint v = menu_compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = menu_compile_shader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "aPos");
    if (with_texcoord) glBindAttribLocation(p, 1, "aTexCoord");
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static const unsigned char* menu_glyph_rows(char c) {
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
    static const unsigned char j_glyph[7] = {1,  1,   1,   1,   1,   17,  14 };
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
    static const unsigned char underscore_glyph[7] = {0, 0, 0, 0, 0, 0, 31};
    switch (c) {
        case 'a': return a_glyph; case 'b': return b_glyph; case 'c': return c_glyph;
        case 'd': return d_glyph; case 'e': return e_glyph; case 'f': return f_glyph;
        case 'g': return g_glyph; case 'h': return h_glyph; case 'i': return i_glyph;
        case 'j': return j_glyph; case 'k': return k_glyph; case 'l': return l_glyph;
        case 'm': return m_glyph; case 'n': return n_glyph; case 'o': return o_glyph;
        case 'p': return p_glyph; case 'q': return q_glyph; case 'r': return r_glyph;
        case 's': return s_glyph; case 't': return t_glyph; case 'u': return u_glyph;
        case 'v': return v_glyph; case 'w': return w_glyph; case 'x': return x_glyph;
        case 'y': return y_glyph; case 'z': return z_glyph;
        case '0': return zero_glyph;  case '1': return one_glyph;
        case '2': return two_glyph;   case '3': return three_glyph;
        case '4': return four_glyph;  case '5': return five_glyph;
        case '6': return six_glyph;   case '7': return seven_glyph;
        case '8': return eight_glyph; case '9': return nine_glyph;
        case '-': return hyphen_glyph; case ':': return colon_glyph;
        case '&': return amp_glyph;
        case '_': return underscore_glyph;
        default:  return blank;
    }
}

static void menu_append_rect(float* vertices, int* vertex_count, float x, float y, float rw, float rh) {
    float x2 = x + rw, y2 = y + rh;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y2;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y;
    vertices[(*vertex_count)++] = x2; vertices[(*vertex_count)++] = y2;
    vertices[(*vertex_count)++] = x;  vertices[(*vertex_count)++] = y2;
}

static void menu_build_text_vertices(const char* text, float x, float y, float scale, float* vertices, int* vertex_count) {
    float cursor_x = x;
    for (const char* ch = text; *ch != '\0'; ++ch) {
        if (*ch == ' ') { cursor_x += 4.0f * scale; continue; }
        const unsigned char* rows = menu_glyph_rows(*ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (rows[row] & (1 << (4 - col)))
                    menu_append_rect(vertices, vertex_count, cursor_x + col * scale, y + row * scale, scale, scale);
        cursor_x += 6.0f * scale;
    }
}

static float menu_text_width(const char* text, float scale) {
    float w = 0.0f;
    for (const char* ch = text; *ch != '\0'; ++ch)
        w += (*ch == ' ') ? 4.0f * scale : 6.0f * scale;
    return w;
}

static int menu_point_in_rect(float px, float py, float x, float y, float rw, float rh) {
    return px >= x && px <= x + rw && py >= y && py <= y + rh;
}

static void menu_draw_input_box(GLint u_color, float x, float y, float w, float h, int focused) {
    float box[12];
    int bc = 0;
    menu_append_rect(box, &bc, x, y, w, h);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * bc, box);
    glUniform4f(u_color, 0.08f, 0.08f, 0.08f, 0.9f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    float c = focused ? 1.0f : 0.6f;
    glUniform4f(u_color, c, c, c, 1.0f);
    float brd = 2.0f;
    float edges[4][4] = {
        {x, y, w, brd},
        {x, y + h - brd, w, brd},
        {x, y, brd, h},
        {x + w - brd, y, brd, h},
    };
    for (int i = 0; i < 4; i++) {
        bc = 0;
        menu_append_rect(box, &bc, edges[i][0], edges[i][1], edges[i][2], edges[i][3]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * bc, box);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

static void menu_window_to_drawable(SDL_Window* window, int wx, int wy, float* dx, float* dy) {
    int ww = 1, wh = 1, dw = 1, dh = 1;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    *dx = (float)wx * (ww > 0 ? (float)dw / (float)ww : 1.0f);
    *dy = (float)wy * (wh > 0 ? (float)dh / (float)wh : 1.0f);
}

static int menu_scan_worlds(MenuWorld* worlds) {
    int count = 0;
    DIR* d = opendir("Savefiles");
    if (!d) return 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL && count < MENU_MAX_WORLDS) {
        if (ent->d_name[0] == '.') continue;
        if (strlen(ent->d_name) >= sizeof(worlds[count].name)) continue;
        char path[300];
        snprintf(path, sizeof(path), "Savefiles/%.63s", ent->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(worlds[count].name, sizeof(worlds[count].name), "%.63s", ent->d_name);
        snprintf(worlds[count].dir, sizeof(worlds[count].dir), "%.99s", path);
        count++;
    }
    closedir(d);
    return count;
}

static void menu_next_world_name(char* name, size_t name_size) {
    for (int i = 1; i < 1000; i++) {
        snprintf(name, name_size, "world%d", i);
        char path[300];
        snprintf(path, sizeof(path), "Savefiles/%s", name);
        struct stat st;
        if (stat(path, &st) != 0) return;
    }
}

static void menu_read_world_cfg(const char* dir, uint32_t* seed, int* natural) {
    *seed = 0;
    *natural = 0;
    char path[300];
    snprintf(path, sizeof(path), "%s/world.cfg", dir);
    FILE* f = fopen(path, "r");
    if (!f) return;
    unsigned s = 0;
    int nat = 0;
    if (fscanf(f, "%u %d", &s, &nat) == 2) {
        *seed = (uint32_t)s;
        *natural = nat ? 1 : 0;
    }
    fclose(f);
}

static int menu_create_world(MenuResult* result, const char* name_text, const char* seed_text, int natural) {
    char name[64];
    if (name_text[0]) {
        snprintf(name, sizeof(name), "%.23s", name_text);
        char path[300];
        snprintf(path, sizeof(path), "Savefiles/%s", name);
        struct stat st;
        if (stat(path, &st) == 0) return 0;
    } else {
        menu_next_world_name(name, sizeof(name));
    }
    snprintf(result->world_dir, sizeof(result->world_dir), "Savefiles/%s", name);
    mkdir(result->world_dir, 0755);
    result->seed = (uint32_t)strtoul(seed_text[0] ? seed_text : "0", NULL, 10);
    result->natural = natural;
    char path[300];
    snprintf(path, sizeof(path), "%s/world.cfg", result->world_dir);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%u %d\n", result->seed, result->natural);
        fclose(f);
    }
    return 1;
}

static int menu_world_name_valid(const char* name) {
    if (!name || !name[0]) return 0;
    for (const char* c = name; *c; c++) {
        char ch = *c;
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        if (!ok) return 0;
    }
    return 1;
}

static int menu_world_exists(const char* name) {
    char path[300];
    snprintf(path, sizeof(path), "Savefiles/%s", name);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int menu_delete_recursive(const char* path) {
    DIR* d = opendir(path);
    if (!d) return 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char child[512];
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!menu_delete_recursive(child)) { closedir(d); return 0; }
        } else {
            if (unlink(child) != 0) { closedir(d); return 0; }
        }
    }
    closedir(d);
    return rmdir(path) == 0;
}

static int menu_rename_world(const char* old_name, const char* new_name) {
    char old_path[300], new_path[300];
    snprintf(old_path, sizeof(old_path), "Savefiles/%s", old_name);
    snprintf(new_path, sizeof(new_path), "Savefiles/%s", new_name);
    return rename(old_path, new_path) == 0;
}

void menu_run(SDL_Window* window, MenuResult* result) {
    memset(result, 0, sizeof(*result));
    mkdir("Savefiles", 0755);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);

    MenuWorld worlds[MENU_MAX_WORLDS];
    int world_count = menu_scan_worlds(worlds);
    int selected_world = world_count > 0 ? 0 : -1;

    const char* color_vert =
        "#version 120\n"
        "attribute vec2 aPos;\n"
        "uniform vec2 screenSize;\n"
        "void main() {\n"
        "   vec2 ndc = vec2((aPos.x/screenSize.x)*2.0-1.0, 1.0-(aPos.y/screenSize.y)*2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";
    const char* color_frag =
        "#version 120\n"
        "uniform vec4 uColor;\n"
        "void main() { gl_FragColor = uColor; }\n";
    const char* tex_vert =
        "#version 120\n"
        "attribute vec2 aPos;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 texCoord;\n"
        "uniform vec2 screenSize;\n"
        "void main() {\n"
        "   vec2 ndc = vec2((aPos.x/screenSize.x)*2.0-1.0, 1.0-(aPos.y/screenSize.y)*2.0);\n"
        "   gl_Position = vec4(ndc, 0.0, 1.0); texCoord = aTexCoord;\n"
        "}\n";
    const char* tex_frag =
        "#version 120\n"
        "varying vec2 texCoord;\n"
        "uniform sampler2D tex0;\n"
        "uniform vec4 tint;\n"
        "void main() { gl_FragColor = texture2D(tex0, texCoord) * tint; }\n";

    GLuint colorProgram = menu_create_program(color_vert, color_frag, 0);
    GLint u_col_screen = glGetUniformLocation(colorProgram, "screenSize");
    GLint u_col_color  = glGetUniformLocation(colorProgram, "uColor");
    GLuint texProgram = menu_create_program(tex_vert, tex_frag, 1);
    GLint u_tex_screen = glGetUniformLocation(texProgram, "screenSize");
    GLint u_tex_tex    = glGetUniformLocation(texProgram, "tex0");
    GLint u_tex_tint   = glGetUniformLocation(texProgram, "tint");

    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16384, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint dirtTexture = 0;
    {
        int w, h, ch;
        unsigned char* bytes = stbi_load("src/textures/dirtblock.png", &w, &h, &ch, 4);
        if (bytes) {
            glGenTextures(1, &dirtTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dirtTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
            stbi_image_free(bytes);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    GLuint buttonTexture = 0;
    {
        int w, h, ch;
        unsigned char* bytes = stbi_load("src/UI/button.png", &w, &h, &ch, 4);
        if (bytes) {
            glGenTextures(1, &buttonTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, buttonTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
            stbi_image_free(bytes);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    GLuint logoTexture = 0;
    int logoW = 0, logoH = 0;
    {
        int ch;
        unsigned char* bytes = stbi_load("src/UI/logo.png", &logoW, &logoH, &ch, 4);
        if (bytes) {
            glGenTextures(1, &logoTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, logoTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logoW, logoH, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
            stbi_image_free(bytes);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    int screen = MENU_SCREEN_MAIN;
    char name_text[MENU_NAME_MAX + 1] = "";
    int name_len = 0;
    char seed_text[MENU_SEED_MAX + 1] = "";
    int seed_len = 0;
    char rename_text[MENU_NAME_MAX + 1] = "";
    int rename_len = 0;
    int focus_field = 0;
    int name_taken = 0;
    int rename_error = 0;
    int done = 0;
    result->quit = 0;
    int suppress_left_release = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SDL_StartTextInput();

    while (!done) {
        int screen_w = 0, screen_h = 0;
        SDL_GL_GetDrawableSize(window, &screen_w, &screen_h);
        glViewport(0, 0, screen_w, screen_h);

        float btn_w = 420.0f, btn_h = 56.0f, btn_gap = 16.0f;
        float btn_x = ((float)screen_w - btn_w) * 0.5f;
        float title_y = (float)screen_h * 0.08f;
        float list_y = (float)screen_h * 0.24f;
        float action_play_y = (float)screen_h - (btn_h * 4.0f + btn_gap * 3.0f + 40.0f);
        float action_rename_y = action_play_y + btn_h + btn_gap;
        float action_delete_y = action_rename_y + btn_h + btn_gap;
        float create_y = action_delete_y + btn_h + btn_gap;
        float name_y = (float)screen_h * 0.22f;
        float seed_y = name_y + btn_h + 46.0f;
        float empty_y = seed_y + btn_h + 46.0f;
        float natural_y = empty_y + btn_h + btn_gap;
        float back_y = natural_y + btn_h + btn_gap;
        float rename_apply_y = seed_y + btn_h + 46.0f;
        float rename_back_y = rename_apply_y + btn_h + btn_gap;

        int list_max = (int)((action_play_y - list_y - 20.0f) / (btn_h + btn_gap));
        if (list_max < 0) list_max = 0;
        if (list_max > world_count) list_max = world_count;

        if (selected_world >= world_count) selected_world = world_count - 1;
        if (world_count == 0) selected_world = -1;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { result->quit = 1; done = 1; }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                glViewport(0, 0, w, h);
            }

            if (event.type == SDL_TEXTINPUT && (screen == MENU_SCREEN_CREATE || screen == MENU_SCREEN_RENAME)) {
                for (const char* c = event.text.text; *c; c++) {
                    char ch = *c;
                    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
                    if (screen == MENU_SCREEN_CREATE) {
                        if (focus_field == 1) {
                            int valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
                            if (valid && name_len < MENU_NAME_MAX) {
                                name_text[name_len++] = ch;
                                name_text[name_len] = '\0';
                                name_taken = 0;
                            }
                        } else if (focus_field == 2) {
                            if (ch >= '0' && ch <= '9' && seed_len < MENU_SEED_MAX) {
                                seed_text[seed_len++] = ch;
                                seed_text[seed_len] = '\0';
                            }
                        }
                    } else if (focus_field == 1) {
                        int valid = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
                        if (valid && rename_len < MENU_NAME_MAX) {
                            rename_text[rename_len++] = ch;
                            rename_text[rename_len] = '\0';
                            rename_error = 0;
                        }
                    }
                }
            }

            if (event.type == SDL_KEYDOWN && (screen == MENU_SCREEN_CREATE || screen == MENU_SCREEN_RENAME)) {
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    if (screen == MENU_SCREEN_CREATE) {
                        if (focus_field == 1 && name_len > 0) { name_text[--name_len] = '\0'; name_taken = 0; }
                        if (focus_field == 2 && seed_len > 0) seed_text[--seed_len] = '\0';
                    } else if (focus_field == 1 && rename_len > 0) {
                        rename_text[--rename_len] = '\0';
                        rename_error = 0;
                    }
                }
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    screen = MENU_SCREEN_MAIN;
                    focus_field = 0;
                    name_taken = 0;
                    rename_error = 0;
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                if (suppress_left_release) {
                    suppress_left_release = 0;
                    continue;
                }
                float mx, my;
                menu_window_to_drawable(window, event.button.x, event.button.y, &mx, &my);

                if (screen == MENU_SCREEN_MAIN) {
                    for (int i = 0; i < list_max; i++) {
                        float wy = list_y + (float)i * (btn_h + btn_gap);
                        if (menu_point_in_rect(mx, my, btn_x, wy, btn_w, btn_h)) selected_world = i;
                    }
                    if (menu_point_in_rect(mx, my, btn_x, action_play_y, btn_w, btn_h) &&
                        selected_world >= 0 && selected_world < world_count) {
                        snprintf(result->world_dir, sizeof(result->world_dir), "%s", worlds[selected_world].dir);
                        menu_read_world_cfg(worlds[selected_world].dir, &result->seed, &result->natural);
                        done = 1;
                    } else if (menu_point_in_rect(mx, my, btn_x, action_rename_y, btn_w, btn_h) &&
                               selected_world >= 0 && selected_world < world_count) {
                        snprintf(rename_text, sizeof(rename_text), "%.20s", worlds[selected_world].name);
                        rename_len = (int)strlen(rename_text);
                        focus_field = 1;
                        rename_error = 0;
                        screen = MENU_SCREEN_RENAME;
                    } else if (menu_point_in_rect(mx, my, btn_x, action_delete_y, btn_w, btn_h) &&
                               selected_world >= 0 && selected_world < world_count) {
                        if (menu_delete_recursive(worlds[selected_world].dir)) {
                            world_count = menu_scan_worlds(worlds);
                            if (world_count <= 0) selected_world = -1;
                            else if (selected_world >= world_count) selected_world = world_count - 1;
                        }
                    } else if (menu_point_in_rect(mx, my, btn_x, create_y, btn_w, btn_h)) {
                        screen = MENU_SCREEN_CREATE;
                        name_len = 0;
                        name_text[0] = '\0';
                        seed_len = 0;
                        seed_text[0] = '\0';
                        focus_field = 0;
                        name_taken = 0;
                    }
                } else if (screen == MENU_SCREEN_CREATE) {
                    if (menu_point_in_rect(mx, my, btn_x, name_y, btn_w, btn_h)) focus_field = 1;
                    else if (menu_point_in_rect(mx, my, btn_x, seed_y, btn_w, btn_h)) focus_field = 2;
                    else focus_field = 0;
                    if (menu_point_in_rect(mx, my, btn_x, empty_y, btn_w, btn_h)) {
                        if (menu_create_world(result, name_text, seed_text, 0)) done = 1;
                        else name_taken = 1;
                    } else if (menu_point_in_rect(mx, my, btn_x, natural_y, btn_w, btn_h)) {
                        if (menu_create_world(result, name_text, seed_text, 1)) done = 1;
                        else name_taken = 1;
                    } else if (menu_point_in_rect(mx, my, btn_x, back_y, btn_w, btn_h)) {
                        screen = MENU_SCREEN_MAIN;
                        focus_field = 0;
                        name_taken = 0;
                    }
                } else {
                    if (menu_point_in_rect(mx, my, btn_x, name_y, btn_w, btn_h)) focus_field = 1;
                    else focus_field = 0;

                    if (menu_point_in_rect(mx, my, btn_x, rename_apply_y, btn_w, btn_h) &&
                        selected_world >= 0 && selected_world < world_count) {
                        if (!menu_world_name_valid(rename_text)) {
                            rename_error = 1;
                        } else if (strcmp(rename_text, worlds[selected_world].name) == 0) {
                            rename_error = 0;
                            screen = MENU_SCREEN_MAIN;
                            focus_field = 0;
                        } else if (menu_world_exists(rename_text)) {
                            rename_error = 2;
                        } else if (!menu_rename_world(worlds[selected_world].name, rename_text)) {
                            rename_error = 3;
                        } else {
                            world_count = menu_scan_worlds(worlds);
                            selected_world = -1;
                            for (int i = 0; i < world_count; i++)
                                if (strcmp(worlds[i].name, rename_text) == 0) { selected_world = i; break; }
                            if (selected_world < 0 && world_count > 0) selected_world = 0;
                            rename_error = 0;
                            screen = MENU_SCREEN_MAIN;
                            focus_field = 0;
                        }
                    } else if (menu_point_in_rect(mx, my, btn_x, rename_back_y, btn_w, btn_h)) {
                        rename_error = 0;
                        screen = MENU_SCREEN_MAIN;
                        focus_field = 0;
                    }
                }
            }
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (dirtTexture) {
            float tile = 64.0f;
            float u1 = (float)screen_w / tile;
            float v1 = (float)screen_h / tile;
            float bg[24] = {
                0.0f,            0.0f,            0.0f, 0.0f,
                (float)screen_w, 0.0f,            u1,   0.0f,
                (float)screen_w, (float)screen_h, u1,   v1,
                0.0f,            0.0f,            0.0f, 0.0f,
                (float)screen_w, (float)screen_h, u1,   v1,
                0.0f,            (float)screen_h, 0.0f, v1,
            };
            glUseProgram(texProgram);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bg), bg);
            glUniform2f(u_tex_screen, (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dirtTexture);
            glUniform1i(u_tex_tex, 0);
            glUniform4f(u_tex_tint, 0.45f, 0.45f, 0.45f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        if (screen == MENU_SCREEN_MAIN && logoTexture && logoW > 0 && logoH > 0) {
            float max_w = (float)screen_w * 0.60f;
            float max_h = (float)screen_h * 0.13f;
            float sx = max_w / (float)logoW;
            float sy = max_h / (float)logoH;
            float scale = sx < sy ? sx : sy;
            if (scale > 1.0f) scale = 1.0f;
            float rw = (float)logoW * scale;
            float rh = (float)logoH * scale;
            float rx = ((float)screen_w - rw) * 0.5f;
            float ry = title_y;
            float lv[24] = {
                rx,      ry,      0.0f, 0.0f,
                rx + rw, ry,      1.0f, 0.0f,
                rx + rw, ry + rh, 1.0f, 1.0f,
                rx,      ry,      0.0f, 0.0f,
                rx + rw, ry + rh, 1.0f, 1.0f,
                rx,      ry + rh, 0.0f, 1.0f,
            };
            glUseProgram(texProgram);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lv), lv);
            glUniform2f(u_tex_screen, (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, logoTexture);
            glUniform1i(u_tex_tex, 0);
            glUniform4f(u_tex_tint, 1.0f, 1.0f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        float button_xs[12], button_ys[12];
        const char* button_labels[12];
        int button_count = 0;
        if (screen == MENU_SCREEN_MAIN) {
            for (int i = 0; i < list_max; i++) {
                button_xs[button_count] = btn_x;
                button_ys[button_count] = list_y + (float)i * (btn_h + btn_gap);
                button_labels[button_count] = worlds[i].name;
                button_count++;
                if (button_count >= 8) break;
            }
            button_xs[button_count] = btn_x;
            button_ys[button_count] = action_play_y;
            button_labels[button_count] = "play";
            button_count++;
            button_xs[button_count] = btn_x;
            button_ys[button_count] = action_rename_y;
            button_labels[button_count] = "rename";
            button_count++;
            button_xs[button_count] = btn_x;
            button_ys[button_count] = action_delete_y;
            button_labels[button_count] = "delete";
            button_count++;
            button_xs[button_count] = btn_x;
            button_ys[button_count] = create_y;
            button_labels[button_count] = "create new world";
            button_count++;
        } else if (screen == MENU_SCREEN_CREATE) {
            button_xs[0] = btn_x; button_ys[0] = empty_y;   button_labels[0] = "empty world";
            button_xs[1] = btn_x; button_ys[1] = natural_y; button_labels[1] = "natural world";
            button_xs[2] = btn_x; button_ys[2] = back_y;    button_labels[2] = "back";
            button_count = 3;
        } else {
            button_xs[0] = btn_x; button_ys[0] = rename_apply_y; button_labels[0] = "apply rename";
            button_xs[1] = btn_x; button_ys[1] = rename_back_y;  button_labels[1] = "back";
            button_count = 2;
        }

        if (buttonTexture) {
            glUseProgram(texProgram);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glUniform2f(u_tex_screen, (float)screen_w, (float)screen_h);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, buttonTexture);
            glUniform1i(u_tex_tex, 0);
            glUniform4f(u_tex_tint, 1.0f, 1.0f, 1.0f, 1.0f);
            for (int b = 0; b < button_count; b++) {
                float bx = button_xs[b], by = button_ys[b];
                float bv[24] = {
                    bx,         by,         0.0f, 0.0f,
                    bx + btn_w, by,         1.0f, 0.0f,
                    bx + btn_w, by + btn_h, 1.0f, 1.0f,
                    bx,         by,         0.0f, 0.0f,
                    bx + btn_w, by + btn_h, 1.0f, 1.0f,
                    bx,         by + btn_h, 0.0f, 1.0f,
                };
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        glUseProgram(colorProgram);
        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glUniform2f(u_col_screen, (float)screen_w, (float)screen_h);

        if (screen == MENU_SCREEN_CREATE || screen == MENU_SCREEN_RENAME) {
            menu_draw_input_box(u_col_color, btn_x, name_y, btn_w, btn_h, focus_field == 1);
            if (screen == MENU_SCREEN_CREATE)
                menu_draw_input_box(u_col_color, btn_x, seed_y, btn_w, btn_h, focus_field == 2);
        }

        float text_verts[16384];
        int tc;
        float title_scale = 4.0f;
        const char* title = screen == MENU_SCREEN_MAIN ? "t400craft" :
                            (screen == MENU_SCREEN_CREATE ? "create new world" : "rename world");
        if (!(screen == MENU_SCREEN_MAIN && logoTexture)) {
            tc = 0;
            menu_build_text_vertices(title, ((float)screen_w - menu_text_width(title, title_scale)) * 0.5f, title_y, title_scale, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glUniform4f(u_col_color, 1.0f, 1.0f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);
        }

        float label_scale = 2.2f;
        glUniform4f(u_col_color, 0.15f, 0.15f, 0.15f, 1.0f);
        for (int b = 0; b < button_count; b++) {
            tc = 0;
            float tw = menu_text_width(button_labels[b], label_scale);
            menu_build_text_vertices(button_labels[b], button_xs[b] + (btn_w - tw) * 0.5f,
                                     button_ys[b] + (btn_h - 7.0f * label_scale) * 0.5f, label_scale, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);
        }

        if (screen == MENU_SCREEN_MAIN && world_count == 0) {
            const char* none = "no worlds yet";
            tc = 0;
            menu_build_text_vertices(none, ((float)screen_w - menu_text_width(none, label_scale)) * 0.5f, list_y, label_scale, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glUniform4f(u_col_color, 0.85f, 0.85f, 0.85f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);
        }

        if (screen == MENU_SCREEN_MAIN && selected_world >= 0 && selected_world < list_max) {
            float outline[48];
            int oc = 0;
            float sy = list_y + (float)selected_world * (btn_h + btn_gap);
            float b = 3.0f;
            menu_append_rect(outline, &oc, btn_x - b, sy - b, btn_w + b * 2.0f, b);
            menu_append_rect(outline, &oc, btn_x - b, sy + btn_h, btn_w + b * 2.0f, b);
            menu_append_rect(outline, &oc, btn_x - b, sy, b, btn_h);
            menu_append_rect(outline, &oc, btn_x + btn_w, sy, b, btn_h);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * oc, outline);
            glUniform4f(u_col_color, 0.95f, 0.95f, 0.95f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, oc / 2);
        }

        if (screen == MENU_SCREEN_CREATE) {
            int cursor_on = (SDL_GetTicks() / 500) % 2 == 0;
            glUniform4f(u_col_color, 1.0f, 1.0f, 1.0f, 1.0f);
            tc = 0;
            menu_build_text_vertices("name:", btn_x, name_y - 28.0f, 2.0f, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            tc = 0;
            menu_build_text_vertices("seed:", btn_x, seed_y - 28.0f, 2.0f, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);

            char shown[MENU_NAME_MAX + 2];
            snprintf(shown, sizeof(shown), "%s%s", name_text,
                     (focus_field == 1 && cursor_on) ? "_" : "");
            tc = 0;
            menu_build_text_vertices(shown, btn_x + 12.0f, name_y + (btn_h - 7.0f * label_scale) * 0.5f, label_scale, text_verts, &tc);
            if (tc > 0) {
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
                glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            }
            snprintf(shown, sizeof(shown), "%s%s", seed_text,
                     (focus_field == 2 && cursor_on) ? "_" : "");
            tc = 0;
            menu_build_text_vertices(shown, btn_x + 12.0f, seed_y + (btn_h - 7.0f * label_scale) * 0.5f, label_scale, text_verts, &tc);
            if (tc > 0) {
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
                glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            }
            if (name_taken) {
                const char* taken = "name already exists";
                tc = 0;
                menu_build_text_vertices(taken, btn_x, name_y + btn_h + 8.0f, 1.8f, text_verts, &tc);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
                glUniform4f(u_col_color, 1.0f, 0.35f, 0.35f, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            }
        } else if (screen == MENU_SCREEN_RENAME) {
            int cursor_on = (SDL_GetTicks() / 500) % 2 == 0;
            glUniform4f(u_col_color, 1.0f, 1.0f, 1.0f, 1.0f);
            tc = 0;
            menu_build_text_vertices("new name:", btn_x, name_y - 28.0f, 2.0f, text_verts, &tc);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
            glDrawArrays(GL_TRIANGLES, 0, tc / 2);

            char shown[MENU_NAME_MAX + 2];
            snprintf(shown, sizeof(shown), "%s%s", rename_text,
                     (focus_field == 1 && cursor_on) ? "_" : "");
            tc = 0;
            menu_build_text_vertices(shown, btn_x + 12.0f, name_y + (btn_h - 7.0f * label_scale) * 0.5f, label_scale, text_verts, &tc);
            if (tc > 0) {
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
                glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            }

            const char* err = NULL;
            if (rename_error == 1) err = "invalid name";
            else if (rename_error == 2) err = "name already exists";
            else if (rename_error == 3) err = "rename failed";
            if (err) {
                tc = 0;
                menu_build_text_vertices(err, btn_x, name_y + btn_h + 8.0f, 1.8f, text_verts, &tc);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * tc, text_verts);
                glUniform4f(u_col_color, 1.0f, 0.35f, 0.35f, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, tc / 2);
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        SDL_GL_SwapWindow(window);
        SDL_Delay(12);
    }

    SDL_StopTextInput();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    if (dirtTexture) glDeleteTextures(1, &dirtTexture);
    if (buttonTexture) glDeleteTextures(1, &buttonTexture);
    if (logoTexture) glDeleteTextures(1, &logoTexture);
    glDeleteProgram(colorProgram);
    glDeleteProgram(texProgram);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &textVAO);
}