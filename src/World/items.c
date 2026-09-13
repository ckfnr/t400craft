#include "items.h"
#include <stddef.h>

const ItemDef item_defs[ITEM_COUNT] = {
    {NULL,                            BLOCK_AIR,              0, 64, 1},
    {"src/textures/cobblestone.png", BLOCK_COBBLESTONE,      0, 64, 1},
    {"src/textures/oak_planks.png",  BLOCK_OAK_PLANKS,       0, 64, 1},
    {"src/textures/grass_side.png",  BLOCK_GRASS,            0, 64, 1},
    {"src/textures/dirtblock.png",   BLOCK_DIRT,             0, 64, 1},
    {"src/textures/water_bucket.png", BLOCK_AIR,             1,  1, 1},
    {"src/textures/oaklog_side.png", BLOCK_OAK_LOG,          0, 64, 1},
    {"src/textures/oak_leaves.png",  BLOCK_OAK_LEAVES,       0, 64, 1},
    {"src/textures/glass_block.png", BLOCK_GLASS,            0, 64, 1},
    {"src/textures/stone.png",       BLOCK_NATURAL_STONE,    0, 64, 1},
    {"src/textures/stone_bricks.png", BLOCK_STONE_BRICKS,    0, 64, 1},
    {"src/textures/smooth_stone.png", BLOCK_SMOOTH_STONE,    0, 64, 1},
    {"src/textures/sand.png",        BLOCK_SAND,             0, 64, 1},
    {"src/textures/obsidian.png",    BLOCK_OBSIDIAN,         0, 64, 1},
    {"src/textures/gravel.png",      BLOCK_GRAVEL,           0, 64, 1},
    {"src/textures/grass_path_side.png", BLOCK_GRASS_PATH,   0, 64, 1},
    {"src/textures/end_stone.png",   BLOCK_ENDSTONE,          0, 64, 1},
    {"src/textures/end_stone_bricks.png", BLOCK_ENDSTONE_BRICKS, 0, 64, 1},
    {"src/textures/purple_stained_glass.png", BLOCK_PURPLE_STAINED_GLASS, 0, 64, 1},
    {"src/textures/blue_stained_glass.png", BLOCK_BLUE_STAINED_GLASS, 0, 64, 1},
    {"src/textures/green_stained_glass.png", BLOCK_GREEN_STAINED_GLASS, 0, 64, 1},
    {"src/textures/red_stained_glass.png", BLOCK_RED_STAINED_GLASS, 0, 64, 1},
};

/* Recipes are ordered lists of occupied grid cells; shaped matching is not
 * needed yet, so the same ingredients may be placed anywhere in the grid. */
const Recipe item_recipes[RECIPE_COUNT] = {
    {{{6, 1}}, 2, 4},
    {{{9, 1}}, 1, 1},
    {{{9, 1}, {9, 1}, {9, 1}, {9, 1}}, 10, 4},
};

const ItemDef* item_get(int item) {
    return item >= 0 && item < ITEM_COUNT ? &item_defs[item] : &item_defs[0];
}

int item_for_block(BlockType block) {
    if (block == BLOCK_STONE) return 9;
    for (int i = 1; i < ITEM_COUNT; i++)
        if (item_defs[i].block == block) return i;
    return 0;
}

int item_hardness_for_block(BlockType block) {
    for (int i = 1; i < ITEM_COUNT; i++)
        if (item_defs[i].block == block) return item_defs[i].hardness;
    return 1;
}

int item_stack_limit(int item) {
    return item_get(item)->stack_limit;
}

int item_find_recipe(const int* grid_items, const int* grid_counts, int grid_size) {
    if (!grid_items || !grid_counts || grid_size < CRAFTING_GRID_SIZE) return -1;
    for (int r = 0; r < RECIPE_COUNT; r++) {
        int used[CRAFTING_GRID_SIZE] = {0};
        int matched = 1;
        int grid_used = 0;
        int recipe_used = 0;
        for (int i = 0; i < CRAFTING_GRID_SIZE; i++)
            if (grid_items[i] != 0) grid_used++;
        for (int i = 0; i < CRAFTING_GRID_SIZE; i++)
            if (item_recipes[r].ingredients[i].item != 0) recipe_used++;
        if (grid_used != recipe_used) continue;
        for (int i = 0; i < CRAFTING_GRID_SIZE; i++) {
            if (grid_items[i] == 0) continue;
            int found = -1;
            for (int j = 0; j < CRAFTING_GRID_SIZE; j++)
                if (!used[j] && item_recipes[r].ingredients[j].item == grid_items[i] &&
                    item_recipes[r].ingredients[j].count <= grid_counts[i]) {
                    found = j;
                    break;
                }
            if (found < 0) { matched = 0; break; }
            used[found] = 1;
        }
        if (matched) return r;
    }
    return -1;
}
