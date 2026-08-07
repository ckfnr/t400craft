#ifndef MENU_H
#define MENU_H

#include <SDL2/SDL.h>
#include <stdint.h>

typedef struct {
    char world_dir[256];
    uint32_t seed;
    int natural;
    int quit;
} MenuResult;

void menu_run(SDL_Window* window, MenuResult* result);

#endif