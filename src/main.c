#include <stdio.h>
#include <string.h>

#include "../md4c/md4c.h"
#include "raylib.h"
#include "utils.h"

typedef enum {
    MD_NODE_DOCUMENT = 0,
} MDNodeType;

typedef struct MDNode MDNode;

struct MDNode {
    MDNodeType type;

    MDNode *next;
};

typedef struct {
    Arena *arena;
    MDNode *docNode;
} ParserData;

const char *test_md = "# Title";

void *alloc_node(ParserData *parserData) {
    return arena_alloc(parserData->arena, sizeof(MDNode));
}

int handle_enter_block(MD_BLOCKTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;

    switch(type) {
        case MD_BLOCK_DOC:
            parserData->docNode = alloc_node(parserData);
            parserData->docNode->type = MD_NODE_DOCUMENT;
            break;
        case MD_BLOCK_H:
            MD_BLOCK_H_DETAIL *hDetail = detail;
            TraceLog(LOG_INFO, "Entering Header with level: %u", hDetail->level);
            break;
        default:
            TraceLog(LOG_ERROR, "Block type not supported");
    }
    return 0;
}

int handle_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    (void) detail;
    (void) userdata;
    TraceLog(LOG_INFO, "Leaving block: %d", type);
    return 0;
}

int handle_enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    (void) detail;
    (void) userdata;
    TraceLog(LOG_INFO, "Entering span: %d", type);
    return 0;
}

int handle_leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    (void) detail;
    (void) userdata;
    TraceLog(LOG_INFO, "Leaving span: %d", type);
    return 0;
}

int handle_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    switch(type) {
        case MD_TEXT_NORMAL:
            printf("Text with size %u: ", size);
            for(size_t i = 0; i < size; i++) {
                putchar(text[i]);
            }
            printf("\n");
            break;
        default:
            TraceLog(LOG_ERROR, "Text type not supported");
    }
    return 0;
}

int main() {
    InitWindow(1280, 720, "C Markdown Renderer");
    SetTargetFPS(60);

    Arena *mdArena = arena_create();

    MD_PARSER parser = {
        .abi_version = 0,
        .flags = MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_LATEXMATHSPANS | MD_FLAG_WIKILINKS,

        .enter_block = &handle_enter_block,
        .leave_block = &handle_leave_block,
        .enter_span = &handle_enter_span,
        .leave_span = &handle_leave_span,
        .text = &handle_text,
    };

    ParserData data = {
        .arena = mdArena,
    };

    md_parse(test_md, strlen(test_md), &parser, &data);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RED);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
