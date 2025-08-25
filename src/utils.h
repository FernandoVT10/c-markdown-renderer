#ifndef UTILS_H
#define UTILS_H

#define ARENA_REGION_SIZE 4096
#define MAX_STACK_SIZE 100

typedef struct ArenaRegion ArenaRegion;

struct ArenaRegion {
    void *data;
    size_t count;
    size_t capacity;

    ArenaRegion *next;
};

typedef struct {
    ArenaRegion *head, *tail;
    size_t count;
} Arena;


typedef struct {
    void *items[MAX_STACK_SIZE];
    size_t count;
} Stack;

Arena *arena_create();
void arena_free(Arena *arena);
void *arena_alloc(Arena *arena, size_t bytes);

void stack_push(Stack *stack, void *item);
void *stack_pop(Stack *stack);
void *stack_get_last(Stack *stack);

#endif // UTILS_H
