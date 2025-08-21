#include <stdio.h>
#include <string.h>

#include "../md4c/md4c.h"
#include "raylib.h"
#include "utils.h"

typedef enum {
    MD_DOCUMENT_NODE = 0,
    MD_HEADER_NODE,
    MD_TEXT_NODE,
} MDNodeType;

typedef struct MDNode MDNode;

typedef struct {
    MDNode *head, *tail;
    size_t count;
} MDNodeList;

typedef struct {
    unsigned int level;
} MDHeaderNode;

struct MDNode {
    MDNodeType type;
    MDNodeList children;
    MDNode *next;

    union {
        MDHeaderNode header;
        char *text; // null-terminated string
    };
};

typedef struct {
    Arena *arena;
    MDNode *docNode;
    Stack parentStack;
} ParserData;

const char *test_md = "# Title\n## Title 2";

MDNode *alloc_node(ParserData *parserData, MDNodeType type) {
    MDNode *node = arena_alloc(parserData->arena, sizeof(MDNode));
    node->type = type;
    return node;
}

void add_children_to_node(MDNode *parent, MDNode *child) {
    if(parent->children.count == 0) {
        parent->children.head = child;
    } else {
        parent->children.tail->next = child;
    }

    parent->children.tail = child;

    parent->children.count++;
}

int handle_enter_block(MD_BLOCKTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;

    if(type == MD_BLOCK_DOC) {
        parserData->docNode = alloc_node(parserData, MD_DOCUMENT_NODE);
        stack_push(&parserData->parentStack, parserData->docNode);
        return 0;
    }

    MDNode *parentNode = stack_get_last(&parserData->parentStack);

    if(parentNode == NULL) {
        TraceLog(LOG_ERROR, "There's no items in the parentStack (block type: %d)", type);
        return 0;
    }

    MDNode *node = NULL;

    switch(type) {
        case MD_BLOCK_DOC:
            TraceLog(LOG_ERROR, "This should be unreachable");
            return 0;
        case MD_BLOCK_H: {
            node = alloc_node(parserData, MD_HEADER_NODE);
            node->header.level = ((MD_BLOCK_H_DETAIL *)detail)->level;

            stack_push(&parserData->parentStack, node);
        } break;
        default:
            TraceLog(LOG_ERROR, "Block type not supported");
    }

    add_children_to_node(parentNode, node);

    return 0;
}

int handle_leave_block(MD_BLOCKTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;
    if(stack_pop(&parserData->parentStack) == NULL) {
        TraceLog(LOG_ERROR, "There's no items in the parentStack, but it's leaving a block");
    }
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

int handle_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userData) {
    ParserData *parserData = userData;
    MDNode *parentNode = stack_get_last(&parserData->parentStack);

    if(parentNode == NULL) {
        TraceLog(LOG_ERROR, "There's no items in the parentStack");
        return 0;
    }

    switch(type) {
        case MD_TEXT_NORMAL:
            MDNode *textNode = alloc_node(parserData, MD_TEXT_NODE);

            // copy the text into the ode
            textNode->text = arena_alloc(parserData->arena, size + 1);
            memcpy(textNode->text, text, size);
            textNode->text[size] = '\0';

            add_children_to_node(parentNode, textNode);
            break;
        default:
            TraceLog(LOG_ERROR, "Text type not supported");
    }
    return 0;
}

void print_indent(int indent) {
    for(int i = 0; i < indent; i++) {
        putchar(' ');
    }
}

void print_md_node(MDNode *node, int indent);

void print_md_node_children(MDNodeList children, int indent) {
    MDNode *node = children.head;
    while(node != NULL) {
        print_md_node(node, indent);
        node = node->next;
    }
}

void print_md_node(MDNode *node, int indent) {
    print_indent(indent);
    switch(node->type) {
        case MD_DOCUMENT_NODE:
            printf("DOCUMENT {\n");
            print_md_node_children(node->children, indent + 4);
            printf("}\n");
            break;
        case MD_HEADER_NODE:
            printf("HEADER(%u) {\n", node->header.level);
            print_md_node_children(node->children, indent + 4);
            print_indent(indent);
            printf("}\n");
            break;
        case MD_TEXT_NODE:
            printf("TEXT(%s)\n", node->text);
            break;
    }
}

void draw_node(MDNode *node);

void draw_node_children(MDNodeList children) {
    MDNode *child = children.head;
    while(child != NULL) {
        draw_node(child);
        child = child->next;
    }
}

void draw_node(MDNode *node) {
    switch(node->type) {
        case MD_DOCUMENT_NODE:
            draw_node_children(node->children);
            break;
        case MD_HEADER_NODE:
            draw_node_children(node->children);
            break;
        case MD_TEXT_NODE:
            draw_text_node(node);
            break;
    }
}

int main() {
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

    // print_md_node(data.docNode, 0);
    // return 0;

    InitWindow(1280, 720, "C Markdown Renderer");
    SetTargetFPS(60);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RED);

        draw_node(data.docNode);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
