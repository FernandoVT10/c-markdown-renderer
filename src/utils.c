#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "utils.h"

static ArenaRegion *alloc_region() {
    ArenaRegion *region = malloc(sizeof(ArenaRegion));
    region->data = malloc(ARENA_REGION_SIZE);
    region->count = 0;
    region->capacity = ARENA_REGION_SIZE;
    region->next = NULL;

    return region;
}

Arena *arena_create() {
    Arena *arena = malloc(sizeof(Arena));
    arena->head = arena->tail = alloc_region();
    arena->count = 1;
    return arena;
}

void arena_free(Arena *arena) {
    ArenaRegion *region = arena->head;

    while(region != NULL) {
        free(region->data);
        ArenaRegion *oldRegion = region;
        region = region->next;
        free(oldRegion);
    }

    free(arena);
}

static ArenaRegion *arena_add_new_region(Arena *arena) {
    ArenaRegion *region = alloc_region();
    arena->tail->next = region;
    arena->tail = region;
    return region;
}

void *arena_alloc(Arena *arena, size_t bytes) {
    assert(bytes <= ARENA_REGION_SIZE && "That's too much memory you are asking for :(");

    ArenaRegion *region = arena->tail;

    // if the region is full we create a new one
    if(region->count + bytes > region->capacity) {
        region = arena_add_new_region(arena);
    }

    void *mem = region->data + region->count;
    bzero(mem, bytes);
    region->count += bytes;
    return mem;
}

void stack_push(Stack *stack, void *item) {
    assert(stack->count < MAX_STACK_SIZE && "Max stack size reached");
    stack->items[stack->count] = item;
    stack->count++;
}

void *stack_pop(Stack *stack) {
    if(stack->count == 0) return NULL;
    return stack->items[--stack->count];
}

void *stack_get_last(Stack *stack) {
    if(stack->count == 0) return NULL;
    return stack->items[stack->count - 1];
}
