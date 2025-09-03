#include <string.h>
#include <stdio.h>

#include "draw.h"
#include "raylib.h"

#define SCREEN_PADDING 20 // separation between the content and the screen
#define DEFAULT_FONT_SIZE 20
#define DEFAULT_PADDING_BETWEEN_BLOCKS 20

#define LIST_DOT_RADIUS 2
#define LIST_LEFT_PADDING 20
// padding between list items
#define LIST_ITEM_PADDING 10
#define LIST_PADDING_AFTER_MARK 10

#define FONT_NORMAL_FILE "./fonts/JetBrainsMono-Regular.ttf"
#define FONT_BOLD_FILE "./fonts/JetBrainsMono-Bold.ttf"

const int HEADER_FONT_SIZES[] = {
    DEFAULT_FONT_SIZE * 2, // level 1
    DEFAULT_FONT_SIZE * 1.75, // level 2
    DEFAULT_FONT_SIZE * 1.5, // level 3
    DEFAULT_FONT_SIZE * 1.25, // level 4
    DEFAULT_FONT_SIZE * 1, // level 5
    DEFAULT_FONT_SIZE * 0.8, // level 6
};

typedef enum {
    FONT_WEIGHT_NORMAL,
    FONT_WEIGHT_BOLD,
} FontWeight;

typedef struct {
    Vector2 pos; // drawing current pos
    float prevHeight;

    struct {
        Font normal;
        Font bold;
    } fonts;
} DrawCtx;

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

DrawCtx ctx = {0};

static void draw_node(MDNode *node, DrawStyle style);

static void draw_node_children(MDNodeList children, DrawStyle style) {
    MDNode *child = children.head;
    while(child != NULL) {
        draw_node(child, style);
        child = child->next;
    }
}

static void draw_word(const char *word, DrawStyle style, Color color) {
    float spacing = 2;

    Font font = style.weight == FONT_WEIGHT_NORMAL ? ctx.fonts.normal : ctx.fonts.bold;

    Vector2 textSize = MeasureTextEx(font, word, style.fontSize, spacing);
    int padding = style.padding.left + style.padding.right;

    if(ctx.pos.x + textSize.x > GetScreenWidth() - padding) {
        ctx.pos.x = style.padding.left;
        ctx.pos.y += style.fontSize;
    }

    DrawTextEx(font, word, ctx.pos, style.fontSize, spacing, color);

    ctx.pos.x += textSize.x;
}

static void draw_text_node(MDNode *textNode, DrawStyle style) {
    static char word[1024];

    Color color = WHITE;

    size_t prevStart = 0;
    for(size_t i = 0; i < strlen(textNode->text); i++) {
        if(textNode->text[i] == ' ') {
            size_t size = i - prevStart + 1;
            strncpy(word, textNode->text + prevStart, size);
            word[size] = '\0';
            prevStart = i + 1;
            draw_word(word, style, color);
        }
    }

    size_t size = strlen(textNode->text) - prevStart;
    strncpy(word, textNode->text + prevStart, size);
    word[size] = '\0';
    draw_word(word, style, color);
}

static void draw_list_node(MDNode *listNode, DrawStyle style) {
    style.padding.left += LIST_LEFT_PADDING;

    ctx.pos.x = style.padding.left;
    ctx.pos.y += ctx.prevHeight + style.paddingBetweenBlocks;

    MDNode *listItem = listNode->children.head;

    size_t i = 0;

    while(listItem != NULL) {
        if(i > 0) {
            ctx.pos.x = style.padding.left;
            ctx.pos.y += style.fontSize + LIST_ITEM_PADDING;
        }

        if(listNode->list.ordered) {
            static char listMark[20];
            snprintf(listMark, 20, "%lu.", i + listNode->list.startIndex);
            draw_word(listMark, style, WHITE);
            ctx.pos.x += LIST_PADDING_AFTER_MARK;
        } else {
            DrawCircle(ctx.pos.x, ctx.pos.y + style.fontSize / 2, LIST_DOT_RADIUS, WHITE);
            ctx.pos.x += LIST_PADDING_AFTER_MARK;
        }


        DrawStyle localStyle = style;
        localStyle.padding.left += LIST_PADDING_AFTER_MARK;
        localStyle.paddingBetweenBlocks = LIST_ITEM_PADDING;

        draw_node_children(listItem->children, localStyle);

        listItem = listItem->next;
        i++;
    }

    ctx.prevHeight = style.fontSize;
}

static void draw_node(MDNode *node, DrawStyle style) {
    switch(node->type) {
        case MD_DOCUMENT_NODE:
            draw_node_children(node->children, style);
            break;
        case MD_HEADER_NODE:
            ctx.pos.x = style.padding.left;
            ctx.pos.y += ctx.prevHeight + style.paddingBetweenBlocks;

            style.fontSize = HEADER_FONT_SIZES[node->header.level - 1];
            draw_node_children(node->children, style);
            ctx.prevHeight = style.fontSize;
            break;
        case MD_TEXT_NODE:
            draw_text_node(node, style);
            break;
        case MD_P_NODE:
            ctx.pos.x = style.padding.left;
            ctx.pos.y += ctx.prevHeight + style.paddingBetweenBlocks;
            draw_node_children(node->children, style);
            ctx.prevHeight = style.fontSize;
            break;
        case MD_LIST_NODE: draw_list_node(node, style); break;
        // ignore it since it will be handled by MD_LIST_NODE case
        case MD_LIST_ITEM_NODE: break;
        case MD_BOLD_NODE:
            style.weight = FONT_WEIGHT_BOLD;
            draw_node_children(node->children, style);
            break;
    }
}

void draw_init() {
    ctx.fonts.normal = LoadFontEx(FONT_NORMAL_FILE, 50, NULL, 0);
    SetTextureFilter(ctx.fonts.normal.texture, TEXTURE_FILTER_BILINEAR);

    ctx.fonts.bold = LoadFontEx(FONT_BOLD_FILE, 50, NULL, 0);
    SetTextureFilter(ctx.fonts.bold.texture, TEXTURE_FILTER_BILINEAR);
}

void draw_document_node(MDNode *docNode) {
    // reset ctx
    ctx.pos.x = SCREEN_PADDING;

    // here we substract the padding since every node adds it before drawing
    ctx.pos.y = SCREEN_PADDING - DEFAULT_PADDING_BETWEEN_BLOCKS;
    ctx.prevHeight = 0;

    DrawStyle style = {
        .fontSize = DEFAULT_FONT_SIZE,
        .padding = {
            .left = SCREEN_PADDING,
            .right = SCREEN_PADDING,
        },
        .paddingBetweenBlocks = DEFAULT_PADDING_BETWEEN_BLOCKS,
        .weight = FONT_WEIGHT_NORMAL,
    };

    draw_node(docNode, style);
}
