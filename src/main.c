#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../md4c/md4c.h"
#include "raylib.h"
#include "utils.h"

#define PADDING_BETWEEN_BLOCKS 20

#define LIST_DOT_RADIUS 3
#define LIST_LEFT_PADDING 20
// padding between list items
#define LIST_ITEM_PADDING 10

#define DEFAULT_FONT_SIZE 20

const int HEADER_FONT_SIZES[] = {
    DEFAULT_FONT_SIZE * 1.75, // level 1
    DEFAULT_FONT_SIZE * 1.5, // level 2
    DEFAULT_FONT_SIZE * 1.25, // level 3
    DEFAULT_FONT_SIZE * 1, // level 4
    DEFAULT_FONT_SIZE * 0.8, // level 5
    DEFAULT_FONT_SIZE * 0.7, // level 6
};

typedef enum {
    MD_DOCUMENT_NODE = 0,
    MD_HEADER_NODE,
    MD_TEXT_NODE,
    MD_P_NODE,
    MD_LIST_NODE,
    MD_LIST_ITEM_NODE,
} MDNodeType;

typedef struct MDNode MDNode;

typedef struct {
    MDNode *head, *tail;
    size_t count;
} MDNodeList;

typedef struct {
    unsigned int level;
} MDHeaderNode;

typedef struct {
    bool ordered;
    unsigned int startIndex; // from where a ordered list starts
} MDListNode;

struct MDNode {
    MDNodeType type;
    MDNodeList children;
    MDNode *next;

    union {
        MDHeaderNode header;
        char *text; // null-terminated string
        MDListNode list;
    };
};

typedef struct {
    Arena *arena;
    MDNode *docNode;
    Stack parentStack;
} ParserData;

const char *test_md = "# Title 1\n## Title 2\n ### Title 3\n- first\n\t- first-first\n- second\n- Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras posuere odio mollis mi semper condimentum. Ut ac lectus non neque sodales laoreet. Vestibulum ex est, vestibulum nec hendrerit fermentum, consequat.\n1. one\n2. two\n3. three";


// const char *test_md = "# Title 1";

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
        case MD_BLOCK_P:
            node = alloc_node(parserData, MD_P_NODE);
            stack_push(&parserData->parentStack, node);
            break;
        case MD_BLOCK_UL:
            node = alloc_node(parserData, MD_LIST_NODE);
            node->list.ordered = false;
            stack_push(&parserData->parentStack, node);
            break;
        case MD_BLOCK_LI:
            node = alloc_node(parserData, MD_LIST_ITEM_NODE);
            stack_push(&parserData->parentStack, node);
            break;
        default:
            TraceLog(LOG_ERROR, "Block type (%d) not supported", type);
            return 0;
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
        case MD_P_NODE:
            printf("PARAGRAPH {");
            print_md_node_children(node->children, indent + 4);
            print_indent(indent);
            printf("}\n");
            break;
        default:
            TraceLog(LOG_ERROR, "Node (%d) not implemented yet", node->type);
    }
}

typedef struct {
    // drawing current pos
    Vector2 pos;
} DrawCtx;

typedef struct {
    int fontSize;

    // this padding will be used to separate the content from the borders of the screen
    struct {
        int left;
        int right;
    } padding;
} DrawStyle;

void draw_node(MDNode *node, DrawCtx *ctx, DrawStyle style);

void draw_node_children(MDNodeList children, DrawCtx *ctx, DrawStyle style) {
    MDNode *child = children.head;
    while(child != NULL) {
        draw_node(child, ctx, style);
        child = child->next;
    }
}

void draw_word(const char *word, DrawCtx *ctx, DrawStyle style, Color color) {
    int textWidth = MeasureText(word, style.fontSize);
    int padding = style.padding.left + style.padding.right;

    if(ctx->pos.x + textWidth > GetScreenWidth() - padding) {
        ctx->pos.x = style.padding.left;
        ctx->pos.y += style.fontSize;
    }

    DrawText(word, ctx->pos.x, ctx->pos.y, style.fontSize, color);

    ctx->pos.x += textWidth;
}

void draw_text_node(MDNode *textNode, DrawCtx *ctx, DrawStyle style) {
    static char word[1024];

    Color color = WHITE;

    size_t prevStart = 0;
    for(size_t i = 0; i < strlen(textNode->text); i++) {
        if(textNode->text[i] == ' ') {
            size_t size = i - prevStart + 1;
            strncpy(word, textNode->text + prevStart, size);
            word[size] = '\0';
            prevStart = i + 1;
            draw_word(word, ctx, style, color);
        }
    }

    size_t size = strlen(textNode->text) - prevStart;
    strncpy(word, textNode->text + prevStart, size);
    word[size] = '\0';
    draw_word(word, ctx, style, color);
}

void draw_node(MDNode *node, DrawCtx *ctx, DrawStyle style) {
    switch(node->type) {
        case MD_DOCUMENT_NODE:
            draw_node_children(node->children, ctx, style);
            break;
        case MD_HEADER_NODE:
            style.fontSize = HEADER_FONT_SIZES[node->header.level - 1];
            draw_node_children(node->children, ctx, style);
            ctx->pos.x = style.padding.left;
            ctx->pos.y += style.fontSize + PADDING_BETWEEN_BLOCKS;
            break;
        case MD_TEXT_NODE:
            draw_text_node(node, ctx, style);
            break;
        case MD_P_NODE:
            draw_node_children(node->children, ctx, style);
            ctx->pos.x = style.padding.left;
            ctx->pos.y += style.fontSize + PADDING_BETWEEN_BLOCKS;
            break;
        case MD_LIST_NODE: {
            MDNode *listItem = node->children.head;
            ctx->pos.x += LIST_LEFT_PADDING;

            size_t i = 0;

            int oldPaddingLeft = style.padding.left;

            style.padding.left += LIST_LEFT_PADDING;

            while(listItem != NULL) {
                if(i > 0) {
                    ctx->pos.x = style.padding.left;
                    ctx->pos.y += style.fontSize + LIST_ITEM_PADDING;
                }

                int paddingAfterDot = 10;

                DrawCircle(ctx->pos.x, ctx->pos.y + style.fontSize / 2, LIST_DOT_RADIUS, WHITE);
                ctx->pos.x += paddingAfterDot;

                DrawStyle localStyle = style;
                localStyle.padding.left += paddingAfterDot;

                draw_node_children(listItem->children, ctx, localStyle);

                listItem = listItem->next;
                i++;
            }

            ctx->pos.x = oldPaddingLeft;
            ctx->pos.y += style.fontSize + PADDING_BETWEEN_BLOCKS;
        } break;
        // ignore it since it will be handled by MD_LIST_NODE case
        case MD_LIST_ITEM_NODE: break;
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
        ClearBackground(BLACK);

        int padding = 20;
        DrawCtx ctx = {
            .pos = {padding, padding},
        };
        DrawStyle style = {
            .fontSize = DEFAULT_FONT_SIZE,
            .padding = {
                .left = padding,
                .right = padding,
            },
        };
        draw_node(data.docNode, &ctx, style);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
