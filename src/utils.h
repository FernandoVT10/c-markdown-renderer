#ifndef UTILS_H
#define UTILS_H

#define ARENA_REGION_SIZE 4096

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

Arena *arena_create();
void arena_free(Arena *arena);
void *arena_alloc(Arena *arena, size_t bytes);

#endif // UTILS_H
