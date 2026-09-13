#ifndef ITEMS_H
#define ITEMS_H

#include "block.h"

#define ITEM_COUNT 22
#define ITEM_STACK_LIMIT 64
#define CRAFTING_GRID_WIDTH 2
#define CRAFTING_GRID_SIZE (CRAFTING_GRID_WIDTH * CRAFTING_GRID_WIDTH)
#define RECIPE_COUNT 3

/*
 * Hardness is the amount of mining work required by a tool.  A value of 1
 * means the block can currently be mined with bare hands.  Tools will later
 * provide mining power that can be compared with this value.
 */
typedef struct {
    const char* texture_path;
    BlockType block;
    int is_bucket;
    int stack_limit;
    int hardness;
} ItemDef;

typedef struct {
    int item;
    int count;
} RecipeIngredient;

typedef struct {
    RecipeIngredient ingredients[CRAFTING_GRID_SIZE];
    int result_item;
    int result_count;
} Recipe;

extern const ItemDef item_defs[ITEM_COUNT];
extern const Recipe item_recipes[RECIPE_COUNT];

const ItemDef* item_get(int item);
int item_for_block(BlockType block);
int item_hardness_for_block(BlockType block);
int item_stack_limit(int item);
int item_find_recipe(const int* grid_items, const int* grid_counts, int grid_size);

#endif
