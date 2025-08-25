#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "../md4c/md4c.h"
#include "raylib.h"
#include "utils.h"

#define DEFAULT_PADDING_BETWEEN_BLOCKS 20

#define LIST_DOT_RADIUS 2
#define LIST_LEFT_PADDING 20
// padding between list items
#define LIST_ITEM_PADDING 10
#define LIST_PADDING_AFTER_MARK 10

#define DEFAULT_FONT_SIZE 20

const int HEADER_FONT_SIZES[] = {
    DEFAULT_FONT_SIZE * 2, // level 1
    DEFAULT_FONT_SIZE * 1.75, // level 2
    DEFAULT_FONT_SIZE * 1.5, // level 3
    DEFAULT_FONT_SIZE * 1.25, // level 4
    DEFAULT_FONT_SIZE * 1, // level 5
    DEFAULT_FONT_SIZE * 0.8, // level 6
};

typedef enum {
    MD_DOCUMENT_NODE = 0,
    MD_HEADER_NODE,
    MD_TEXT_NODE,
    MD_P_NODE,
    MD_LIST_NODE,
    MD_LIST_ITEM_NODE,
    MD_BOLD_NODE,
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
        case MD_BLOCK_OL:
            node = alloc_node(parserData, MD_LIST_NODE);
            node->list.ordered = true;
            node->list.startIndex = ((MD_BLOCK_OL_DETAIL*)detail)->start;
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

int handle_enter_span(MD_SPANTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;
    MDNode *parentNode = stack_get_last(&parserData->parentStack);

    if(parentNode == NULL) {
        TraceLog(LOG_ERROR, "There's no items in the parentStack (span type: %d)", type);
        return 0;
    }

    MDNode *node = NULL;

    switch(type) {
        case MD_SPAN_STRONG:
            node = alloc_node(parserData, MD_BOLD_NODE);
            stack_push(&parserData->parentStack, node);
            break;
        default:
            TraceLog(LOG_ERROR, "Span type (%d) not supported", type);
            break;
    }

    add_children_to_node(parentNode, node);

    return 0;
}

int handle_leave_span(MD_SPANTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;
    if(stack_pop(&parserData->parentStack) == NULL) {
        TraceLog(LOG_ERROR, "There's no items in the parentStack, but it's leaving a span");
    }
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
    float prevHeight;
    struct {
        Font normal;
        Font bold;
    } fonts;
} DrawCtx;

typedef enum {
    FONT_WEIGHT_NORMAL,
    FONT_WEIGHT_BOLD,
} FontWeight;

typedef struct {
    int fontSize;

    // this padding will be used to separate the content from the borders of the screen
    struct {
        int left;
        int right;
    } padding;

    int paddingBetweenBlocks;

    FontWeight weight;
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
    float spacing = 2;

    Font font = style.weight == FONT_WEIGHT_NORMAL ? ctx->fonts.normal : ctx->fonts.bold;

    Vector2 textSize = MeasureTextEx(font, word, style.fontSize, spacing);
    int padding = style.padding.left + style.padding.right;

    if(ctx->pos.x + textSize.x > GetScreenWidth() - padding) {
        ctx->pos.x = style.padding.left;
        ctx->pos.y += style.fontSize;
    }

    DrawTextEx(font, word, ctx->pos, style.fontSize, spacing, color);

    ctx->pos.x += textSize.x;
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
            ctx->pos.x = style.padding.left;
            ctx->pos.y += ctx->prevHeight + style.paddingBetweenBlocks;

            style.fontSize = HEADER_FONT_SIZES[node->header.level - 1];
            draw_node_children(node->children, ctx, style);
            ctx->prevHeight = style.fontSize;
            break;
        case MD_TEXT_NODE:
            draw_text_node(node, ctx, style);
            break;
        case MD_P_NODE:
            ctx->pos.x = style.padding.left;
            ctx->pos.y += ctx->prevHeight + style.paddingBetweenBlocks;
            draw_node_children(node->children, ctx, style);
            ctx->prevHeight = style.fontSize;
            break;
        case MD_LIST_NODE: {
            style.padding.left += LIST_LEFT_PADDING;
            ctx->pos.x = style.padding.left;
            ctx->pos.y += ctx->prevHeight + style.paddingBetweenBlocks;

            MDNode *listItem = node->children.head;

            size_t i = 0;

            while(listItem != NULL) {
                if(i > 0) {
                    ctx->pos.x = style.padding.left;
                    ctx->pos.y += style.fontSize + LIST_ITEM_PADDING;
                }

                if(node->list.ordered) {
                    static char listMark[20];
                    snprintf(listMark, 20, "%lu.", i + node->list.startIndex);
                    draw_word(listMark, ctx, style, WHITE);
                    ctx->pos.x += LIST_PADDING_AFTER_MARK;
                } else {
                    DrawCircle(ctx->pos.x, ctx->pos.y + style.fontSize / 2, LIST_DOT_RADIUS, WHITE);
                    ctx->pos.x += LIST_PADDING_AFTER_MARK;
                }


                DrawStyle localStyle = style;
                localStyle.padding.left += LIST_PADDING_AFTER_MARK;
                localStyle.paddingBetweenBlocks = LIST_ITEM_PADDING;

                draw_node_children(listItem->children, ctx, localStyle);

                listItem = listItem->next;
                i++;
            }

            ctx->prevHeight = style.fontSize;
        } break;
        // ignore it since it will be handled by MD_LIST_NODE case
        case MD_LIST_ITEM_NODE: break;
        case MD_BOLD_NODE:
            style.weight = FONT_WEIGHT_BOLD;
            draw_node_children(node->children, ctx, style);
            break;
    }
}

char *read_file(const char *filePath) {
    FILE *filePtr = fopen(filePath, "r");

    if(filePtr == NULL) {
        switch(errno) {
            case ENOENT:
                fprintf(stderr, "%s: No such file or directory", filePath);
                break;
            case EACCES:
                fprintf(stderr, "%s: Permission denied", filePath);
                break;
            default:
                fprintf(stderr, "%s: Couldn't open the file (errno: %d)", filePath, errno);
                break;
        }
        return NULL;
    }

    fseek(filePtr, 0, SEEK_END);
    long length = ftell(filePtr);
    rewind(filePtr);

    if(length == -1 || (unsigned long) length >= SIZE_MAX) {
        fprintf(stderr, "%s: The file size is too big", filePath);
        return NULL;
    }

    size_t ulength = (size_t) length;
    char *text = malloc(ulength + 1);

    fread(text, 1, ulength, filePtr);

    text[ulength] = '\0';

    return text;
}

int main(int argc, const char **args) {
    if(argc != 2) {
        printf("Usage: ./main <file-path>\n");
        return 1;
    }

    const char *filePath = args[1];
    const char *mdText = read_file(filePath);
    if(mdText == NULL) {
        return 1;
    }

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

    md_parse(mdText, strlen(mdText), &parser, &data);

    InitWindow(1280, 720, "C Markdown Renderer");
    SetTargetFPS(60);

    DrawCtx ctx = {
        .fonts = {
            .normal = LoadFontEx("./fonts/JetBrainsMono-Regular.ttf", 50, NULL, 0),
            .bold = LoadFontEx("./fonts/JetBrainsMono-Bold.ttf", 50, NULL, 0),
        },
    };

    SetTextureFilter(ctx.fonts.normal.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ctx.fonts.bold.texture, TEXTURE_FILTER_BILINEAR);

    Camera2D camera = {
        .zoom = 1,
    };

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // reset ctx
        int padding = 20;
        ctx.pos.x = padding;
        ctx.pos.y = padding;
        ctx.prevHeight = -padding;

        DrawStyle style = {
            .fontSize = DEFAULT_FONT_SIZE,
            .padding = {
                .left = padding,
                .right = padding,
            },
            .paddingBetweenBlocks = DEFAULT_PADDING_BETWEEN_BLOCKS,
            .weight = FONT_WEIGHT_NORMAL,
        };

        BeginMode2D(camera);
        draw_node(data.docNode, &ctx, style);
        EndMode2D();

        if(IsKeyDown(KEY_DOWN) && ctx.pos.y > GetScreenHeight()) {
            camera.offset.y -= 10;

            float bottom = GetScreenHeight() - ctx.pos.y - padding * 2;

            if(camera.offset.y < bottom) {
                camera.offset.y = bottom;
            }
        } else if(IsKeyDown(KEY_UP)) {
            camera.offset.y += 10;

            if(camera.offset.y > 0) {
                camera.offset.y = 0;
            }
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
