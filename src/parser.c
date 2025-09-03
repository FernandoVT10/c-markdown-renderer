#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../md4c/md4c.h"
#include "parser.h"
#include "raylib.h"

#define LogError(level, msg) log_error(level,  msg, __FILE__, __LINE__);

#define UNREACHABLE() TraceLog(LOG_FATAL, "%s:%d: Unreachable code", __FILE__, __LINE__)

void log_error(TraceLogLevel level, const char *msg, const char *file, int line) {
    TraceLog(level, "%s:%d: %s", file, line, msg);
}

static char *read_file(const char *filePath) {
    FILE *filePtr = fopen(filePath, "r");

    if(filePtr == NULL) {
        switch(errno) {
            case ENOENT:
                LogError(LOG_ERROR, "No such file or directory");
                break;
            case EACCES:
                LogError(LOG_ERROR, "Permission denied");
                break;
            default:
                const char *msg = TextFormat("Couldn't open the file (errno: %d)", errno);
                LogError(LOG_ERROR, msg);
                break;
        }
        return NULL;
    }

    fseek(filePtr, 0, SEEK_END);
    long length = ftell(filePtr);
    rewind(filePtr);

    if(length == -1 || (unsigned long) length >= SIZE_MAX) {
        LogError(LOG_ERROR, "The file size is way too big");
        return NULL;
    }

    size_t ulength = (size_t) length;
    char *text = malloc(ulength + 1);

    fread(text, 1, ulength, filePtr);

    text[ulength] = '\0';

    return text;
}

static MDNode *alloc_node(ParserData *parserData, MDNodeType type) {
    MDNode *node = arena_alloc(parserData->arena, sizeof(MDNode));
    node->type = type;
    return node;
}

static void add_children_to_node(MDNode *parent, MDNode *child) {
    if(parent->children.count == 0) {
        parent->children.head = child;
    } else {
        parent->children.tail->next = child;
    }

    parent->children.tail = child;

    parent->children.count++;
}

static MDNode *get_parent_node(ParserData *parserData) {
    MDNode *parentNode = stack_get_last(&parserData->parentStack);

    if(parentNode == NULL) {
        LogError(LOG_ERROR, "There are no parents in the parentStack");
        return NULL;
    }

    return parentNode;
}

static int handle_enter_block(MD_BLOCKTYPE type, void *detail, void *dataPtr) {
    ParserData *parserData = dataPtr;

    if(type == MD_BLOCK_DOC) {
        parserData->docNode = alloc_node(parserData, MD_DOCUMENT_NODE);
        stack_push(&parserData->parentStack, parserData->docNode);
        return 0;
    }

    MDNode *parentNode = get_parent_node(parserData);
    if(parentNode == NULL) {
        return 1;
    }

    MDNode *node = NULL;

    switch(type) {
        case MD_BLOCK_DOC: UNREACHABLE(); break;
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
            LogError(LOG_ERROR, "Block type not supported");
            return 1;
    }

    add_children_to_node(parentNode, node);

    return 0;
}

static int handle_leave_block(MD_BLOCKTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;

    if(parserData->parentStack.count == 0) {
        LogError(LOG_ERROR, "There's no items in the parentStack");
        return 1;
    }

    stack_pop(&parserData->parentStack);
    return 0;
}

static int handle_enter_span(MD_SPANTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;

    MDNode *parentNode = get_parent_node(parserData);
    if(parentNode == NULL) {
        return 1;
    }

    MDNode *node = NULL;

    switch(type) {
        case MD_SPAN_STRONG:
            node = alloc_node(parserData, MD_BOLD_NODE);
            stack_push(&parserData->parentStack, node);
            break;
        default:
            LogError(LOG_ERROR, "Span type not supported");
            return 1;
    }

    add_children_to_node(parentNode, node);

    return 0;
}

static int handle_leave_span(MD_SPANTYPE type, void *detail, void *userData) {
    ParserData *parserData = userData;

    if(parserData->parentStack.count == 0) {
        LogError(LOG_ERROR, "There's no items in the parentStack");
        return 1;
    }

    stack_pop(&parserData->parentStack);
    return 0;
}

static int handle_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userData) {
    ParserData *parserData = userData;
    MDNode *parentNode = get_parent_node(parserData);
    if(parentNode == NULL) {
        return 1;
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
            LogError(LOG_ERROR, "Text type not supported");
            return 1;
    }
    return 0;
}

void parse_file(const char *filePath, ParserData *parserData) {
    char *content = read_file(filePath);

    if(content == NULL) return;

    MD_PARSER parser = {
        .abi_version = 0,
        .flags = MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_LATEXMATHSPANS | MD_FLAG_WIKILINKS,

        .enter_block = &handle_enter_block,
        .leave_block = &handle_leave_block,
        .enter_span = &handle_enter_span,
        .leave_span = &handle_leave_span,
        .text = &handle_text,
    };

    md_parse(content, strlen(content), &parser, parserData);
}
